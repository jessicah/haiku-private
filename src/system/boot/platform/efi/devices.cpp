/*
 * Copyright 2003-2006, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2014-2015, Jessica Hamilton, jessica.l.hamilton@gmail.com.
 *
 * Distributed under the terms of the MIT License.
 */


#include "efi_platform.h"
#include "efigpt.h"

#include "Header.h"
	// from src/add-ons/kernel/partitioning_systems/gpt

#include <KernelExport.h>
#include <boot/platform.h>
#include <boot/partitions.h>
#include <boot/stdio.h>
#include <boot/stage2.h>

#include <ctype.h>
#include <string.h>

//#define TRACE_DEVICES
#ifdef TRACE_DEVICES
#	define TRACE(x) dprintf x
#else
#	define TRACE(x) ;
#endif

#define UUID_LEN	16


static EFI_GUID sBlockIOGuid = BLOCK_IO_PROTOCOL;
static EFI_GUID sDevicePathGuid = DEVICE_PATH_PROTOCOL;
static uint8 sTargetUUID[16];
static bool sDeviceNodesAdded = false;

extern EFI_HANDLE kImage;

typedef struct {
	uint32	data1;
	uint16	data2;
	uint16	data3;
	uint16	data4;
	uint8	data5[6];
} uuid_t;


static EFI_DEVICE_PATH*
get_device_path_of_type(EFI_DEVICE_PATH *devicePath, uint16 type, uint16 subType)
{
	EFI_DEVICE_PATH *node = devicePath;
	while (!IsDevicePathEnd(node)) {
		if (DevicePathType(node) == type
				&& (subType == 0xFFFF || (DevicePathSubType(node) == subType)))
			return node;

		node = NextDevicePathNode(node);
	}

	return NULL;
}


static uint64
device_path_partition_start(EFI_DEVICE_PATH *devicePath)
{
	EFI_DEVICE_PATH *node = get_device_path_of_type(devicePath, MEDIA_DEVICE_PATH, MEDIA_HARDDRIVE_DP);

	if (node != NULL && IsDevicePathEnd(NextDevicePathNode(node))) {
		// this device path represents a partition
		HARDDRIVE_DEVICE_PATH *dp = (HARDDRIVE_DEVICE_PATH*)node;
		return dp->PartitionStart;
	}

	node = get_device_path_of_type(devicePath, MESSAGING_DEVICE_PATH, MSG_SATA_DP);
	if (node == NULL)
		node = get_device_path_of_type(devicePath, MESSAGING_DEVICE_PATH, MSG_ATAPI_DP);
	if (node != NULL && IsDevicePathEnd(NextDevicePathNode(node))) {
		// we have either an ATA[PI] or SATA device node
		return 0;
	}

	return 0;
}


//	#pragma mark -


class UEFIContainer : public Node {
	public:
		UEFIContainer(EFI_BLOCK_IO *blockIO, EFI_DEVICE_PATH *devicePath);
		virtual ~UEFIContainer();

		virtual ssize_t ReadAt(void *cookie, off_t pos, void *buffer, size_t bufferSize);
		virtual ssize_t WriteAt(void *cookie, off_t pos, const void *buffer, size_t bufferSize);

		virtual off_t Size() const { return fSize; }

		uint32 BlockSize() const { return fBlockSize; }
		uint64 Offset() const { return fStart; }

		uint8* UUID();

	protected:
		EFI_BLOCK_IO*		fBlockIO;
		EFI_DEVICE_PATH*	fDevicePath;

		uint32				fBlockSize;
		uint64				fSize;
		uint64				fStart;
};


UEFIContainer::UEFIContainer(EFI_BLOCK_IO *blockIO, EFI_DEVICE_PATH *devicePath)
	:
	fBlockIO(blockIO),
	fDevicePath(devicePath)
{
	TRACE(("UEFIContainer %p created\n", this));

	fBlockSize = fBlockIO->Media->BlockSize;
	fSize = (fBlockIO->Media->LastBlock + 1) * fBlockSize;
	fStart = device_path_partition_start(devicePath);
}


UEFIContainer::~UEFIContainer()
{
	TRACE(("UEFIContainer %p deleted\n", this));
}


ssize_t
UEFIContainer::ReadAt(void *cookie, off_t pos, void *buffer, size_t bufferSize)
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
UEFIContainer::WriteAt(void* cookie, off_t pos, const void* buffer,
	size_t bufferSize)
{
	return B_UNSUPPORTED;
}


uint8*
UEFIContainer::UUID()
{
	if (fStart == 0) {
		// We need to read LBA 1, and get the GPT header to find the UUID
		EFI_PARTITION_TABLE_HEADER *header = (EFI_PARTITION_TABLE_HEADER*)malloc(fBlockSize);
		ssize_t bytesRead = ReadAt(NULL, fBlockSize, header, fBlockSize);
		if (bytesRead != fBlockSize) {
			TRACE(("Unable to read LBA 1 of device! (%p)\n", this));
			free(header);
			return NULL;
		}

		uint8 *uuid = (uint8*)malloc(UUID_LEN);
		memcpy(uuid, (uint8*)&header->DiskGUID, UUID_LEN);
		free(header);
		return uuid;
	}

	// Otherwise, we fetch the signature from the device path
	HARDDRIVE_DEVICE_PATH *node = (HARDDRIVE_DEVICE_PATH*)get_device_path_of_type(fDevicePath,
		MEDIA_DEVICE_PATH, MEDIA_HARDDRIVE_DP);
	if (node == NULL) {
		TRACE(("Failed to get the device path of this partition! (%p)\n", this));
		return NULL;
	}

	uint8 *uuid = (uint8*)malloc(UUID_LEN);
	memcpy(uuid, node->Signature, UUID_LEN);
	return uuid;
}


//	#pragma mark -


static status_t
add_block_devices(NodeList *nodeList)
{
	// As we already add all devices, we don't need to add more
	if (sDeviceNodesAdded) {
		TRACE(("device count = %d\n", nodeList->Count()));
		return nodeList->Count() > 0 ? B_OK : B_ENTRY_NOT_FOUND;
	}

	EFI_BLOCK_IO *blockIO;
	EFI_DEVICE_PATH *devicePath, *node, *targetDevicePath = NULL;
	EFI_HANDLE *handles = NULL;
	EFI_STATUS status;
	UINTN size = 0;

	status = kBootServices->LocateHandle(ByProtocol, &sBlockIOGuid, 0, &size, 0);
	if (status == EFI_BUFFER_TOO_SMALL) {
		handles = (EFI_HANDLE *)malloc(size);
		status = kBootServices->LocateHandle(ByProtocol, &sBlockIOGuid, 0, &size, handles);
	}
	if (status != EFI_SUCCESS) {
		if (handles != NULL)
			free(handles);
		return B_ERROR;
	}

	// If we walk backwards, we should see the partition handles before
	// the device handles... a nasty assumption, but it shall do the trick
	for (int n = (size / sizeof(EFI_HANDLE)) - 1; n >= 0; --n) {
		TRACE(("processing handle %d\n", n));
		status = kBootServices->HandleProtocol(handles[n], &sDevicePathGuid, (void**)&devicePath);
		if (status != EFI_SUCCESS)
			continue;

		node = devicePath;
		// iterate to the last component of the device path
		while (!IsDevicePathEnd(NextDevicePathNode(node)))
			node = NextDevicePathNode(node);

		if (DevicePathType(node) == MEDIA_DEVICE_PATH && DevicePathSubType(node) == MEDIA_HARDDRIVE_DP) {
			HARDDRIVE_DEVICE_PATH *dp = (HARDDRIVE_DEVICE_PATH*)node;
			if (memcmp((uint8*)dp->Signature, sTargetUUID, UUID_LEN) == 0)
				targetDevicePath = get_device_path_of_type(devicePath, MESSAGING_DEVICE_PATH, 0xFFFF);
			continue;
		}

		if (DevicePathType(node) != MESSAGING_DEVICE_PATH)
			continue;

		status = kBootServices->HandleProtocol(handles[n], &sBlockIOGuid, (void **)&blockIO);
		if (status != EFI_SUCCESS)
			continue;

		if (!blockIO->Media->MediaPresent)
			continue;

		UEFIContainer *container = new(std::nothrow)UEFIContainer(blockIO, devicePath);
		if (container == NULL) {
			TRACE(("creating container failed\n"));
			continue;
		}

		bool added = false;

		if (targetDevicePath != NULL) {
			if (DevicePathNodeLength(targetDevicePath) == DevicePathNodeLength(node)
					&& memcmp(targetDevicePath, node, DevicePathNodeLength(node)) == 0) {
				nodeList->InsertBefore(nodeList->Head(), container);
				added = true;
			}
		}

		if (!added)
			nodeList->Add(container);

		TRACE(("is this the same as the first? %s. the last? %s. how many? %d\n",
			nodeList->Head() == container ? "yes" : "no",
			nodeList->Tail() == container ? "yes" : "no",
			nodeList->Count()));
	}

	sDeviceNodesAdded = true;

	return nodeList->Count() > 0 ? B_OK : B_ENTRY_NOT_FOUND;
}


//	#pragma mark -


static bool
parse_uuid(const char *in, uuid_t &out)
{
	const char *cp = in;
	char buf[3];

	// validate the string first
	for (int i = 0; i <= 36; i++, cp++) {
		if ((i == 8) || (i == 13) || (i == 18) || (i == 23)) {
			if (*cp == '-')
				continue;
			else {
				dprintf("expected hyphen at %d\n", i);
				return false;
			}
		}
		if (i == 36) {
			if (*cp != '\0') {
				dprintf("expected terminating nul character at %d\n", i);
				return false;
			}
			continue;
		}
		if (!isxdigit(*cp)) {
			dprintf("expected hexadecimal character at %d\n", i);
			return false;
		}
	}
	out.data1 = strtoul(in, NULL, 16);
	out.data2 = strtoul(in + 9, NULL, 16);
	out.data3 = strtoul(in + 14, NULL, 16);
	out.data4 = strtoul(in + 19, NULL, 16);
	cp = in + 24;
	buf[2] = '\0';
	for (int i = 0; i < 6; ++i) {
		buf[0] = *cp++;
		buf[1] = *cp++;
		out.data5[i] = strtoul(buf, NULL, 16);
	}

	return true;
}


static void
pack_uuid(const uuid_t &uu, uint8 *out)
{
	uint32 tmp;

	// should be 3,2,1,0,5,4,7,6,8,9... but other code has incorrect
	// byte ordering, so we'll temporarily hack it here for the
	// moment :p
	tmp = uu.data1;
	out[0] = (uint8)tmp;
	tmp >>= 8;
	out[1] = (uint8)tmp;
	tmp >>= 8;
	out[2] = (uint8)tmp;
	tmp >>= 8;
	out[3] = (uint8)tmp;

	tmp = uu.data2;
	out[4] = (uint8)tmp;
	tmp >>= 8;
	out[5] = (uint8)tmp;

	tmp = uu.data3;
	out[6] = (uint8)tmp;
	tmp >>= 8;
	out[7] = (uint8)tmp;

	tmp = uu.data4;
	out[9] = (uint8)tmp;
	tmp >>= 8;
	out[8] = (uint8)tmp;

	memcpy(out+10, uu.data5, 6);
}


static char*
from_ucs2(const uint16 *wideString, uint32 length)
{
	char *asciiString = (char*)malloc(length);

	for (uint32 i = 0; i < length; ++i)
		asciiString[i] = (char)(wideString[i] & 0xff);

	return asciiString;
}


status_t
platform_add_boot_device(struct stage2_args *args, NodeList *devicesList)
{
	// Let's see if we can find out some information about our
	// boot invocation
	EFI_STATUS status = EFI_SUCCESS;
	EFI_LOADED_IMAGE* loaded_image = NULL;
	EFI_GUID loaded_image_protocol = LOADED_IMAGE_PROTOCOL;
	status = kBootServices->HandleProtocol(kImage,
			&loaded_image_protocol, (void**)&loaded_image);
	if (status != EFI_SUCCESS || loaded_image == NULL) {
		TRACE(("platform_add_boot_device: can't get loaded image protocol\n"));
		return B_ENTRY_NOT_FOUND;
	}

	// If using efibootmgr, do NOT use the option to store the command-line
	// args as ASCII... the standard is for UCS-2.
	// I suppose we _could_ test if the very first byte is NULL, and then
	// switch between ASCII and UCS-2, but I'd rather follow the standard
	// here.
	char* payload = from_ucs2((CHAR16*)loaded_image->LoadOptions, loaded_image->LoadOptionsSize / 2);

	const char* identifier = "Target(";
	const char* target = payload;
	uint32 length = strlen(identifier);
	for (uint32 i = 0; i < loaded_image->LoadOptionsSize; ++i) {
		bool found = true;
		for (uint32 j = 0; j < length; ++j) {
			if (payload[i+j] != identifier[j]) {
				found = false;
				break;
			}
		}
		if (found) {
			TRACE(("\n\ntarget uuid at offset %u bytes, %u remaining\n",
				i + length, loaded_image->LoadOptionsSize - i + length));
			target += i + length;
			break;
		}
	}

	if (target == payload) {
		dprintf("EXIT: platform_add_boot_device: not adding block devices\n");
		return B_ENTRY_NOT_FOUND;
	}

	uuid_t uu;
	if (!parse_uuid(strndup(target, 36), uu)) {
		TRACE(("platform_add_boot_device: failed to parse uuid\n"));
		return B_ENTRY_NOT_FOUND;
	}
	pack_uuid(uu, sTargetUUID);

	return add_block_devices(devicesList);
}


status_t
platform_get_boot_partition(struct stage2_args *args, Node *bootDevice,
	NodeList *list, boot::Partition **_partition)
{
	NodeIterator it = list->GetIterator();
	while (it.HasNext()) {
		boot::Partition *partition = (boot::Partition *)it.Next();
		EFI::Header *header = (EFI::Header*)partition->content_cookie;
		if (header == NULL)
			continue;

		efi_partition_entry entry = header->EntryAt((addr_t)partition->cookie);
		if (memcmp((uint8*)&entry.unique_guid, sTargetUUID, UUID_LEN) == 0) {
			*_partition = partition;
			return B_OK;
		}
	}

	TRACE(("platform_get_boot_partition: cannot find partition via UUID\n"));
	return B_ENTRY_NOT_FOUND;
}


status_t
platform_add_block_devices(stage2_args *args, NodeList *devicesList)
{
	return add_block_devices(devicesList);
}


status_t
platform_register_boot_device(Node *bootDevice)
{
	UEFIContainer *container = (UEFIContainer*)bootDevice;

	disk_identifier identifier;
	identifier.bus_type = UNKNOWN_BUS;
	identifier.device_type = UNKNOWN_DEVICE;
	identifier.device.unknown.size = container->Size();
	memcpy(identifier.device.unknown.uuid, container->UUID(), UUID_LEN);
	identifier.device.unknown.use_uuid = true;

	gBootVolume.SetData(BOOT_VOLUME_DISK_IDENTIFIER, B_RAW_TYPE,
		&identifier, sizeof(disk_identifier));

	return B_OK;
}
