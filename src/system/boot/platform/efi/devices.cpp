#include "efi_platform.h"

#include <boot/partitions.h>
#include <boot/platform.h>
#include <boot/stage2.h>
#include <boot/stdio.h>


static EFI_DEVICE_PATH*
get_device_path_of_type(EFI_DEVICE_PATH *devicePath, uint16 type, uint16 subType)
{
	EFI_DEVICE_PATH *node = devicePath;
	while (!IsDevicePathEnd(node)) {
		if (DevicePathType(node) == type
			&& (subType == 0xffff || (DevicePathSubType(node) == subType)))
			return node;

		node = NextDevicePathNode(node);
	}

	return NULL;
}


static uint64
device_path_partition_start(EFI_DEVICE_PATH *devicePath)
{
	EFI_DEVICE_PATH *node;

	node = get_device_path_of_type(devicePath,
		MEDIA_DEVICE_PATH, MEDIA_HARDDRIVE_DP);
	if (node != NULL && IsDevicePathEnd(NextDevicePathNode(node))) {
		// this device path represents a partition
		HARDDRIVE_DEVICE_PATH *partition = (HARDDRIVE_DEVICE_PATH*)node;
		return partition->PartitionStart;
	}

	node = get_device_path_of_type(devicePath, MEDIA_DEVICE_PATH,
		MEDIA_CDROM_DP);
	if (node != NULL && IsDevicePathEnd(NextDevicePathNode(node))) {
		CDROM_DEVICE_PATH *cdrom = (CDROM_DEVICE_PATH*)node;
		printf("partition start for cdrom = %ld\n", cdrom->PartitionStart);
		return cdrom->PartitionStart;
	}

	return 0;
}


class EfiContainer : public Node
{
	public:
		EfiContainer(EFI_BLOCK_IO *blockIo, EFI_DEVICE_PATH *devicePath);
		virtual ~EfiContainer();

		virtual ssize_t ReadAt(void *cookie, off_t pos, void *buffer,
			size_t bufferSize);
		virtual ssize_t WriteAt(void *cookie, off_t pos, const void *buffer,
			size_t bufferSize);

		virtual off_t Size() const { return fSize; }

		uint32 BlockSize() const { return fBlockSize; }
		uint64 Offset() const { return fStart; }
	private:
		EFI_BLOCK_IO*		fBlockIo;
		EFI_DEVICE_PATH*	fDevicePath;

		uint32				fBlockSize;
		uint64				fSize;
		uint64				fStart;
};


EfiContainer::EfiContainer(EFI_BLOCK_IO *blockIo, EFI_DEVICE_PATH *devicePath)
	:
	fBlockIo(blockIo),
	fDevicePath(devicePath)
{
	fBlockSize = fBlockIo->Media->BlockSize;
	fSize = (fBlockIo->Media->LastBlock + 1) * fBlockSize;
	fStart = device_path_partition_start(devicePath);
}


EfiContainer::~EfiContainer()
{
}


ssize_t
EfiContainer::ReadAt(void *cookie, off_t pos, void *buffer, size_t bufferSize)
{
	uint32 offset = pos % fBlockSize;
	pos /= fBlockSize;

	uint32 numBlocks = (offset + bufferSize + fBlockSize) / fBlockSize;
	char readBuffer[numBlocks * fBlockSize];

	EFI_STATUS status = fBlockIo->ReadBlocks(fBlockIo, fBlockIo->Media->MediaId,
		pos, sizeof(readBuffer), readBuffer);

	if (status != EFI_SUCCESS)
		return B_ERROR;

	memcpy(buffer, readBuffer + offset, bufferSize);

	return bufferSize;
}


ssize_t
EfiContainer::WriteAt(void *cookie, off_t pos, const void* buffer, size_t bufferSize)
{
	return B_UNSUPPORTED;
}


status_t
platform_add_boot_device(struct stage2_args *args, NodeList *devicesList)
{
	printf("entered platform_add_boot_device\n");
	// called first, add whole devices, not partitions, make the device
	// we want to be considered for booting first
	EFI_GUID blockIoGuid = BLOCK_IO_PROTOCOL;
	EFI_GUID devicePathGuid = DEVICE_PATH_PROTOCOL;
	EFI_BLOCK_IO *blockIo;
	EFI_DEVICE_PATH *devicePath, *node, *targetDevicePath = NULL;
	EFI_HANDLE *handles = NULL;
	EFI_STATUS status;
	UINTN size = 0;

	status = kBootServices->LocateHandle(ByProtocol, &blockIoGuid, 0, &size, 0);
	if (status == EFI_BUFFER_TOO_SMALL) {
		printf("allocating %d bytes for handles\n", size);
		handles = (EFI_HANDLE*)malloc(size);
		status = kBootServices->LocateHandle(ByProtocol, &blockIoGuid, 0, &size, handles);
	}
	if (status != EFI_SUCCESS) {
		printf("failed to get blockio handles\n");
		if (handles != NULL)
			free(handles);
		return B_ERROR;
	}

	for (int n = (size / sizeof(EFI_HANDLE)) - 1; n >= 0; --n) {
		status = kBootServices->HandleProtocol(handles[n], &devicePathGuid,
			(void**)&devicePath);
		if (status != EFI_SUCCESS)
			continue;

		printf("Handle %d: type = %d, subtype = %d, length = %d\n",
			n, DevicePathType(devicePath), DevicePathSubType(devicePath),
			DevicePathNodeLength(devicePath));

		node = devicePath;
		// iterate to the last component of the device path
		while (!IsDevicePathEnd(NextDevicePathNode(node)))
			node = NextDevicePathNode(node);

		printf("Node: type = %d, subtype = %d, length = %d\n",
			DevicePathType(node), DevicePathSubType(node),
			DevicePathNodeLength(node));

		// only look for a bootable CD atm
		if (DevicePathType(node) == MEDIA_DEVICE_PATH
			&& DevicePathSubType(node) == MEDIA_CDROM_DP) {
			printf("found a cdrom device path\n");
			targetDevicePath = get_device_path_of_type(devicePath,
				MESSAGING_DEVICE_PATH, 0xffff);
			CDROM_DEVICE_PATH *cdrom = (CDROM_DEVICE_PATH*)devicePath;
			printf("cdrom partition offset = %d\n", cdrom->PartitionStart);
			continue;
		}

		if (DevicePathType(node) != MESSAGING_DEVICE_PATH)
			continue;

		status = kBootServices->HandleProtocol(handles[n], &blockIoGuid,
			(void**)&blockIo);
		if (status != EFI_SUCCESS)
			continue;

		if (!blockIo->Media->MediaPresent)
			continue;

		EfiContainer *container = new(std::nothrow)EfiContainer(blockIo, devicePath);
		if (container == NULL)
			continue;

		if (targetDevicePath != NULL
			&& memcmp(targetDevicePath, node, DevicePathNodeLength(node)) == 0) {
			printf("added a cdrom! yay\n");
			devicesList->InsertBefore(devicesList->Head(), container);
		}
	}

	return devicesList->Count() > 0 ? B_OK : B_ENTRY_NOT_FOUND;
}


status_t
platform_add_block_devices(struct stage2_args *args, NodeList *devicesList)
{
	printf("entered platform_add_block_devices\n");
	// from mount_file_systems(), add all devices we want to search for
	// Haiku partitions
	return B_ERROR;
}


status_t
platform_get_boot_partition(struct stage2_args *args, Node *bootDevice,
		NodeList *partitions, boot::Partition **_partition)
{
	printf("entered platform_get_boot_partition\n");
	// called second, from our boot device, then find the boot
	// partition on the device that we want to boot from
	NodeIterator it = partitions->GetIterator();
	while (it.HasNext()) {
		printf("beep beep\n");
		boot::Partition *partition = (boot::Partition*)it.Next();
		*_partition = partition;
		// apparently it can't be mounted though...
		return B_OK;
	}

	return B_ERROR;
}


status_t
platform_register_boot_device(Node *device)
{
	printf("entered platform_register_boot_device\n");
	// we've loaded the kernel, and what we decided to boot from, so
	// now all we have left to do is to pass on a disk_identifier
	// that the kernel can use to identify it (again)
	return B_ERROR;
}
