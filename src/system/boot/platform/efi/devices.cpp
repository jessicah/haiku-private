/*
 * Copyright 2003-2006, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2014, Jessica Hamilton, jessica.l.hamilton@gmail.com.
 *
 * Distributed under the terms of the MIT License.
 */


#include "efi_platform.h"

#include <KernelExport.h>
#include <boot/platform.h>
#include <boot/partitions.h>
#include <boot/stdio.h>
#include <boot/stage2.h>

#include <string.h>

//#define TRACE_DEVICES
#ifdef TRACE_DEVICES
#	define TRACE(x) dprintf x
#else
#	define TRACE(x) ;
#endif


static uint8 sDriveIdentifier = 0;
static EFI_GUID sBlockIOGuid = BLOCK_IO_PROTOCOL;
static EFI_GUID sDevicePathGuid = DEVICE_PATH_PROTOCOL;


class EFIBlockDevice : public Node {
	public:
		EFIBlockDevice(EFI_BLOCK_IO *blockIO);
		virtual ~EFIBlockDevice();

		status_t InitCheck() const;

		virtual ssize_t ReadAt(void *cookie, off_t pos, void *buffer, size_t bufferSize);
		virtual ssize_t WriteAt(void *cookie, off_t pos, const void *buffer, size_t bufferSize);

		virtual off_t Size() const;

		uint32 BlockSize() const { return fBlockSize; }

		status_t FillIdentifier();

		disk_identifier &Identifier() { return fIdentifier; }
		uint8 DriveID() const { return fDriveID; }

	protected:
		EFI_BLOCK_IO	*fBlockIO;
		uint8			fDriveID;
		uint64			fSize;
		uint32			fBlockSize;
		disk_identifier	fIdentifier;
};


static bool sBlockDevicesAdded = false;

#if false
// we can actually determine this with EFI... to fix up
static void
check_cd_boot(EFIBlockDevice *device)
{
	gBootVolume.SetInt32(BOOT_METHOD, BOOT_METHOD_HARD_DISK);

	if (device->DriveID() != 0)
		return;

	// we obviously were booted from CD!

	//specification_packet *packet = (specification_packet *)kDataSegmentScratch;
	//if (packet->media_type != 0)
		gBootVolume.SetInt32(BOOT_METHOD, BOOT_METHOD_CD);


}


// think all of this checksum stuff can be removed
// should be able to use the device path to generate a unique identifier


static off_t
get_next_check_sum_offset(int32 index, off_t maxSize)
{
	// The boot block often contains the disk superblock, and should be
	// unique enough for most cases
	if (index < 2)
		return index * 512;

	// Try some data in the first part of the drive
	if (index < 4)
		return (maxSize >> 10) + index * 2048;

	// Some random value might do
	return ((system_time() + index) % (maxSize >> 9)) * 512;
}


/**	Computes a check sum for the specified block.
 *	The check sum is the sum of all data in that block interpreted as an
 *	array of uint32 values.
 *	Note, this must use the same method as the one used in kernel/fs/vfs_boot.cpp.
 */
static uint32
compute_check_sum(EFIBlockDevice *device, off_t offset)
{
	char buffer[512];
	ssize_t bytesRead = device->ReadAt(NULL, offset, buffer, sizeof(buffer));
	if (bytesRead < B_OK)
		return 0;

	if (bytesRead < (ssize_t)sizeof(buffer))
		memset(buffer + bytesRead, 0, sizeof(buffer) - bytesRead);

	uint32 *array = (uint32 *)buffer;
	uint32 sum = 0;

	for (uint32 i = 0; i < (bytesRead + sizeof(uint32) - 1) / sizeof(uint32); i++) {
		sum += array[i];
	}

	return sum;
}


static void
find_unique_check_sums(NodeList *devices)
{
	NodeIterator iterator = devices->GetIterator();
	Node *device;
	int32 index = 0;
	off_t minSize = 0;
	const int32 kMaxTries = 200;

	while (index < kMaxTries) {
		bool clash = false;

		iterator.Rewind();

		while ((device = iterator.Next()) != NULL) {
			EFIBlockDevice *drive = (EFIBlockDevice *)device;
#if 0
			// there is no RTTI in the boot loader...
			BIOSDrive *drive = dynamic_cast<BIOSDrive *>(device);
			if (drive == NULL)
				continue;
#endif

			// TODO: currently, we assume that the BIOS provided us with unique
			//	disk identifiers... hopefully this is a good idea
			if (drive->Identifier().device_type != UNKNOWN_DEVICE)
				continue;

			if (minSize == 0 || drive->Size() < minSize)
				minSize = drive->Size();

			// check for clashes

			NodeIterator compareIterator = devices->GetIterator();
			while ((device = compareIterator.Next()) != NULL) {
				EFIBlockDevice *compareDrive = (EFIBlockDevice *)device;

				if (compareDrive == drive
					|| compareDrive->Identifier().device_type != UNKNOWN_DEVICE)
					continue;

// TODO: Until we can actually get and compare *all* fields of the disk
// identifier in the kernel, we cannot compare the whole structure (we also
// should be more careful zeroing the structure before we fill it).
#if 0
				if (!memcmp(&drive->Identifier(), &compareDrive->Identifier(),
						sizeof(disk_identifier))) {
					clash = true;
					break;
				}
#else
				const disk_identifier& ourId = drive->Identifier();
				const disk_identifier& otherId = compareDrive->Identifier();
				if (memcmp(&ourId.device.unknown.check_sums,
						&otherId.device.unknown.check_sums,
						sizeof(ourId.device.unknown.check_sums)) == 0) {
					clash = true;
				}
#endif
			}

			if (clash)
				break;
		}

		if (!clash) {
			// our work here is done.
			return;
		}

		// add a new block to the check sums

		off_t offset = get_next_check_sum_offset(index, minSize);
		int32 i = index % NUM_DISK_CHECK_SUMS;
		iterator.Rewind();

		while ((device = iterator.Next()) != NULL) {
			EFIBlockDevice *drive = (EFIBlockDevice *)device;

			disk_identifier& disk = drive->Identifier();
			disk.device.unknown.check_sums[i].offset = offset;
			disk.device.unknown.check_sums[i].sum = compute_check_sum(drive, offset);

			TRACE(("disk %x, offset %Ld, sum %lu\n", drive->DriveID(), offset,
				disk.device.unknown.check_sums[i].sum));
		}

		index++;
	}

	// If we get here, we couldn't find a way to differentiate all disks from each other.
	// It's very likely that one disk is an exact copy of the other, so there is nothing
	// we could do, anyway.

	dprintf("Could not make EFI block devices unique! Might boot from the wrong disk...\n");
}
#endif

static status_t
add_block_devices(NodeList *devicesList, bool identifierMissing) // should bool be a reference?
{
	if (sBlockDevicesAdded)
		return B_OK;

	EFI_BLOCK_IO *blockIO;
	EFI_DEVICE_PATH *devicePath, *node;
	EFI_HANDLE *handles, handle;
	EFI_STATUS status;
	UINTN size;

	size = 0;
	handles = NULL;

	status = kBootServices->LocateHandle(ByProtocol, &sBlockIOGuid, 0, &size, 0);
	if (status == EFI_BUFFER_TOO_SMALL) {
		handles = (EFI_HANDLE *)malloc(size);
		status = kBootServices->LocateHandle(ByProtocol, &sBlockIOGuid, 0, &size, handles);
		if (status != EFI_SUCCESS)
			free(handles);
	}
	if (status != EFI_SUCCESS)
		return B_ERROR;

	for (unsigned int n = 0; n < size / sizeof(EFI_HANDLE); n++) {
		status = kBootServices->HandleProtocol(handles[n], &sDevicePathGuid, (void **)&devicePath);
		if (status != EFI_SUCCESS)
			continue;

		node = devicePath;

		while (!IsDevicePathEnd(NextDevicePathNode(node)))
			node = NextDevicePathNode(node);

		status = kBootServices->HandleProtocol(handles[n], &sBlockIOGuid, (void **)&blockIO);
		if (status != EFI_SUCCESS)
			continue;
		if (blockIO->Media->LogicalPartition == false)
			continue;

		/* If we come across a logical partition of subtype CDROM
		 * it doesn't refer to the CD filesystem itself, but rather
		 * to any usable El Torito boot image on it. In this case
		 * we try to find the parent device and add that instead as
		 * that will be the CD fileystem.
		 */
		if (DevicePathType(node) == MEDIA_DEVICE_PATH &&
			DevicePathSubType(node) == MEDIA_CDROM_DP) {
			node->Type = END_DEVICE_PATH_TYPE;
			node->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
			status = kBootServices->LocateDevicePath(&sBlockIOGuid, &devicePath, &handle);
			// TODO: actually care about CD-ROMs
			continue;
		}

		EFIBlockDevice *device = new(std::nothrow) EFIBlockDevice(blockIO);
		if (device->InitCheck() != B_OK) {
			delete device;
			continue;
		}

		devicesList->Add(device);

		if (device->FillIdentifier() != B_OK)
			identifierMissing = true;
	}

	sBlockDevicesAdded = true;
	return B_OK;
}


//	#pragma mark -


EFIBlockDevice::EFIBlockDevice(EFI_BLOCK_IO *blockIO)
	:
	fBlockIO(blockIO),
	fDriveID(++sDriveIdentifier),
	fSize(0)
{
	TRACE(("drive ID %u\n", fDriveID));

	if (fBlockIO->Media->MediaPresent == false)
		return;

	fBlockSize = fBlockIO->Media->BlockSize;
	fSize = (fBlockIO->Media->LastBlock + 1) * fBlockSize;
}


EFIBlockDevice::~EFIBlockDevice()
{
}


status_t
EFIBlockDevice::InitCheck() const
{
	return fSize > 0 ? B_OK : B_ERROR;
}


ssize_t
EFIBlockDevice::ReadAt(void *cookie, off_t pos, void *buffer, size_t bufferSize)
{
	uint32 offset = pos % fBlockSize;
	pos /= fBlockSize;

	uint32 numBlocks = (offset + bufferSize + fBlockSize) / fBlockSize;
	char readBuffer[numBlocks * fBlockSize];

	EFI_STATUS status = fBlockIO->ReadBlocks(fBlockIO, fBlockIO->Media->MediaId,
		pos, sizeof(readBuffer), readBuffer);

	if (status != EFI_SUCCESS)
		return B_ERROR;

	memcpy(buffer, readBuffer + offset, bufferSize);

	return bufferSize;
}


ssize_t
EFIBlockDevice::WriteAt(void* cookie, off_t pos, const void* buffer,
	size_t bufferSize)
{
	return B_UNSUPPORTED;
}


off_t
EFIBlockDevice::Size() const
{
	return fSize;
}


status_t
EFIBlockDevice::FillIdentifier()
{
	fIdentifier.bus_type = UNKNOWN_BUS;
	fIdentifier.device_type = UNKNOWN_DEVICE;
	fIdentifier.device.unknown.size = Size();

	for (int32 i = 0; i < NUM_DISK_CHECK_SUMS; i++) {
		fIdentifier.device.unknown.check_sums[i].offset = -1;
		fIdentifier.device.unknown.check_sums[i].sum = 0;
	}

	return B_ERROR;
}


//	#pragma mark -


status_t
platform_add_boot_device(struct stage2_args *args, NodeList *devicesList)
{
	return B_ENTRY_NOT_FOUND;
}


status_t
platform_get_boot_partition(struct stage2_args *args, Node *bootDevice,
	NodeList *list, boot::Partition **_partition)
{
	return B_ENTRY_NOT_FOUND;
}


status_t
platform_add_block_devices(stage2_args *args, NodeList *devicesList)
{
	return add_block_devices(devicesList, false);
}


status_t
platform_register_boot_device(Node *device)
{
	EFIBlockDevice *drive = (EFIBlockDevice *)device;

	//check_cd_boot(drive);

	gBootVolume.SetInt64("boot drive number", drive->DriveID());
	gBootVolume.SetData(BOOT_VOLUME_DISK_IDENTIFIER, B_RAW_TYPE,
		&drive->Identifier(), sizeof(disk_identifier));

	return B_OK;
}
