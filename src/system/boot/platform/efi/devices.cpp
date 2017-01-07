/*
 * Copyright 2016 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <string.h>

#include <boot/partitions.h>
#include <boot/platform.h>
#include <boot/stage2.h>
#include <boot/stdio.h>
#include <util/list.h>

#include "efi_platform.h"


struct device_handle {
	list_link			link;
	EFI_DEVICE_PATH*	device_path;
	EFI_HANDLE			handle;
};


static struct list sMessagingDevices;
static struct list sMediaDevices;

static EFI_GUID BlockIoGUID = BLOCK_IO_PROTOCOL;
static EFI_GUID LoadedImageGUID = LOADED_IMAGE_PROTOCOL;
static EFI_GUID DevicePathGUID = DEVICE_PATH_PROTOCOL;

#if 0
static UINTN
device_path_length(EFI_DEVICE_PATH* path)
{
	EFI_DEVICE_PATH *node = path;
	UINTN length = 0;
	while (!IsDevicePathEnd(node)) {
		length += DevicePathNodeLength(node);
		node = NextDevicePathNode(node);
	}

	// node now points to the device path end node; add its length as well
	return length + DevicePathNodeLength(node);
}
#endif

#if 0
// If matchSubPath is true, then the second device path can be a sub-path
// of the first device path
static bool
compare_device_paths(EFI_DEVICE_PATH* first, EFI_DEVICE_PATH* second, bool matchSubPath = false)
{
	EFI_DEVICE_PATH *firstNode = first;
	EFI_DEVICE_PATH *secondNode = second;
	while (!IsDevicePathEnd(firstNode) && !IsDevicePathEnd(secondNode)) {
		UINTN firstLength = DevicePathNodeLength(firstNode);
		UINTN secondLength = DevicePathNodeLength(secondNode);
		if (firstLength != secondLength || memcmp(firstNode, secondNode, firstLength) != 0) {
			return false;
		}
		firstNode = NextDevicePathNode(firstNode);
		secondNode = NextDevicePathNode(secondNode);
	}

	if (matchSubPath)
		return IsDevicePathEnd(secondNode);

	return IsDevicePathEnd(firstNode) && IsDevicePathEnd(secondNode);
}
#endif

#if 0
static bool
add_device_path(struct list *list, EFI_DEVICE_PATH* path, EFI_HANDLE handle)
{
	device_handle *node = NULL;
	while ((node = (device_handle*)list_get_next_item(list, node)) != NULL) {
		if (compare_device_paths(node->device_path, path)) {
			dprintf("    device path already exists\n");
			return false;
		}
	}

	dprintf("    adding a new device path!\n");
	UINTN length = device_path_length(path);
	node = (device_handle*)malloc(sizeof(struct device_handle));
	node->device_path = (EFI_DEVICE_PATH*)malloc(length);
	node->handle = handle;
	memcpy(node->device_path, path, length);

	list_add_item(list, node);

	return true;
}
#endif

class EfiDevice : public Node
{
	public:
		EfiDevice(EFI_BLOCK_IO *blockIo, EFI_DEVICE_PATH *devicePath);
		virtual ~EfiDevice();

		virtual ssize_t ReadAt(void *cookie, off_t pos, void *buffer,
			size_t bufferSize);
		virtual ssize_t WriteAt(void *cookie, off_t pos, const void *buffer,
			size_t bufferSize) { return B_UNSUPPORTED; }
		virtual off_t Size() const {
			return (fBlockIo->Media->LastBlock + 1) * BlockSize(); }

		uint32 BlockSize() const { return fBlockIo->Media->BlockSize; }
		bool ReadOnly() const { return fBlockIo->Media->ReadOnly; }
		int32 BootMethod() const {
			if (fDevicePath->Type == MEDIA_DEVICE_PATH) {
				if (fDevicePath->SubType == MEDIA_CDROM_DP)
					return BOOT_METHOD_CD;
				if (fDevicePath->SubType == MEDIA_HARDDRIVE_DP)
					return BOOT_METHOD_HARD_DISK;
			}

			return BOOT_METHOD_DEFAULT;
		}
		EFI_DEVICE_PATH* DevicePath() { return fDevicePath; }
	private:
		EFI_BLOCK_IO*		fBlockIo;
		EFI_DEVICE_PATH*	fDevicePath;
};


EfiDevice::EfiDevice(EFI_BLOCK_IO *blockIo, EFI_DEVICE_PATH *devicePath)
	:
	fBlockIo(blockIo),
	fDevicePath(devicePath)
{
}


EfiDevice::~EfiDevice()
{
}


ssize_t
EfiDevice::ReadAt(void *cookie, off_t pos, void *buffer, size_t bufferSize)
{
	uint32 offset = pos % BlockSize();
	pos /= BlockSize();

	uint32 numBlocks = (offset + bufferSize + BlockSize()) / BlockSize();
	char readBuffer[numBlocks * BlockSize()];

	if (fBlockIo->ReadBlocks(fBlockIo, fBlockIo->Media->MediaId,
		pos, sizeof(readBuffer), readBuffer) != EFI_SUCCESS)
		return B_ERROR;

	memcpy(buffer, readBuffer + offset, bufferSize);

	return bufferSize;
}

#if 0
static EFI_DEVICE_PATH*
find_device_path(EFI_DEVICE_PATH *devicePath, uint16 type, uint16 subType)
{
	EFI_DEVICE_PATH *node = devicePath;
	while (!IsDevicePathEnd(node)) {
		if (DevicePathType(node) == type
			&& (subType == 0xFFFF || DevicePathSubType(node) == subType))
			return node;

		node = NextDevicePathNode(node);
	}

	return NULL;
}
#endif

#if 0
static status_t
build_device_handles()
{
	EFI_GUID blockIoGuid = BLOCK_IO_PROTOCOL;
	EFI_GUID devicePathGuid = DEVICE_PATH_PROTOCOL;

	//EFI_BLOCK_IO *blockIo;
	EFI_DEVICE_PATH *devicePath, *node;
	EFI_HANDLE *handles = NULL;
	EFI_STATUS status;
	UINTN size = 0;

	status = kBootServices->LocateHandle(ByProtocol, &blockIoGuid, 0, &size, 0);
	if (status != EFI_BUFFER_TOO_SMALL)
		return B_ENTRY_NOT_FOUND;

	handles = (EFI_HANDLE*)malloc(size);
	status = kBootServices->LocateHandle(ByProtocol, &blockIoGuid, 0, &size,
		handles);
	if (status != EFI_SUCCESS) {
		free(handles);
		return B_ENTRY_NOT_FOUND;
	}

	for (UINTN n = 0; n < (size / sizeof(EFI_HANDLE)); n++) {
		dprintf("  processing handle %lu\n", n);
		status = kBootServices->HandleProtocol(handles[n], &devicePathGuid,
			(void**)&devicePath);
		if (status != EFI_SUCCESS)
			continue;

		node = devicePath;
		while (!IsDevicePathEnd(NextDevicePathNode(node)))
			node = NextDevicePathNode(node);

		if (DevicePathType(node) == MEDIA_DEVICE_PATH) {
			// Add to our media devices list
			dprintf("    adding a media device path instance\n");
			add_device_path(&sMediaDevices, devicePath, handles[n]);
		} else if (DevicePathType(node) == MESSAGING_DEVICE_PATH) {
			// Add to our messaging devices list
			dprintf("    adding a messaging device path instance\n");
			add_device_path(&sMessagingDevices, devicePath, handles[n]);
		}
		dprintf("  finished pass...\n");
	}
	dprintf("finished processing handles\n");

	return B_OK;
}
#endif

#if 0
		node = devicePath;
		while (!IsDevicePathEnd(NextDevicePathNode(node)))
			node = NextDevicePathNode(node);

		if (DevicePathType(node) == MEDIA_DEVICE_PATH
			&& DevicePathSubType(node) == MEDIA_CDROM_DP) {
			targetDevicePath = find_device_path(devicePath,
				MESSAGING_DEVICE_PATH, 0xFFFF);
			continue;
		}

		if (DevicePathType(node) != MESSAGING_DEVICE_PATH)
			continue;

		status = kBootServices->HandleProtocol(handles[n], &blockIoGuid,
			(void**)&blockIo);
		if (status != EFI_SUCCESS || !blockIo->Media->MediaPresent)
			continue;

		EfiDevice *device = new(std::nothrow)EfiDevice(blockIo, devicePath);
		if (device == NULL)
			continue;

		if (targetDevicePath != NULL
			&& memcmp(targetDevicePath, node, DevicePathNodeLength(node)) == 0)
			devicesList->InsertBefore(devicesList->Head(), device);
		else
			devicesList->Insert(device);

		targetDevicePath = NULL;
	}

	return devicesList->Count() > 0 ? B_OK : B_ENTRY_NOT_FOUND;
}
#endif


static off_t
get_next_check_sum_offset(int32 index, off_t maxSize)
{
	if (index < 2)
		return index * 512;

	if (index < 4)
		return (maxSize >> 10) + index * 2048;

	return ((system_time() + index) % (maxSize >> 9)) * 512;
}


static uint32
compute_check_sum(Node *device, off_t offset)
{
	char buffer[512];
	ssize_t bytesRead = device->ReadAt(NULL, offset, buffer, sizeof(buffer));
	if (bytesRead < B_OK)
		return 0;

	if (bytesRead < (ssize_t)sizeof(buffer))
		memset(buffer + bytesRead, 0, sizeof(buffer) - bytesRead);

	uint32 *array = (uint32*)buffer;
	uint32 sum = 0;

	for (uint32 i = 0; i < (bytesRead + sizeof(uint32) - 1) / sizeof(uint32); i++)
		sum += array[i];

	return sum;
}

#if 0
static status_t
add_boot_device(NodeList *devicesList)
{
	return B_ENTRY_NOT_FOUND;
}
#endif

#if 0
static device_handle*
get_messaging_device_for_media_device(device_handle *media_device)
{
	device_handle *messaging_device = NULL;
	while ((messaging_device = (device_handle*)list_get_next_item(&sMessagingDevices, messaging_device)) != NULL) {
		if (compare_device_paths(media_device->device_path,
				messaging_device->device_path, true)) {
			dprintf("found messaging device for media device\n");
			return messaging_device;
		}
	}

	return NULL;
}
#endif

#if 0
static status_t
add_cd_devices(NodeList *devicesList)
{
	device_handle *handle = NULL;
	while ((handle = (device_handle*)list_get_next_item(&sMediaDevices, handle)) != NULL) {
		EFI_DEVICE_PATH *node = handle->device_path;
		while (!IsDevicePathEnd(NextDevicePathNode(node)))
			node = NextDevicePathNode(node);

		if (DevicePathType(node) != MEDIA_DEVICE_PATH)
			continue;

		if (DevicePathSubType(node) != MEDIA_CDROM_DP)
			continue;

		device_handle *messaging_device = get_messaging_device_for_media_device(handle);
		if (messaging_device == NULL) {
			dprintf("couldn't find messaging device for media device\n");
			continue;
		}

		EFI_BLOCK_IO *blockIo;
		EFI_GUID blockIoGuid = BLOCK_IO_PROTOCOL;
		EFI_STATUS status = kBootServices->HandleProtocol(messaging_device->handle,
			&blockIoGuid, (void**)&blockIo);
		if (status != EFI_SUCCESS) {
			dprintf("unable to get block IO for device path\n");
			continue;
		}
		if (!blockIo->Media->MediaPresent) {
			dprintf("media not present for device path\n");
			continue;
		}

		if (blockIo->Media->ReadOnly) {
			dprintf("note: media is readonly\n");
		}

		EfiDevice *device = new(std::nothrow)EfiDevice(blockIo, handle->device_path);
		if (device == NULL)
			continue;

		dprintf("adding a CD device\n");
		devicesList->Insert(device);
	}

	return devicesList->Count() > 0 ? B_OK : B_ENTRY_NOT_FOUND;
}
#endif

#if 0
static status_t
add_remaining_devices(NodeList *devicesList)
{
	device_handle *node = NULL;
	while ((node = (device_handle*)list_get_next_item(&sMessagingDevices, node)) != NULL) {
		NodeIterator it = devicesList->GetIterator();
		bool found = false;
		while (it.HasNext()) {
			EfiDevice *device = (EfiDevice*)it.Next();
			// device->DevicePath() is a Media Device Path instance
			if (compare_device_paths(device->DevicePath(), node->device_path, true)) {
				found = true;
				break;
			}
		}
		if (!found) {
			EFI_BLOCK_IO *blockIo;
			EFI_GUID blockIoGuid = BLOCK_IO_PROTOCOL;
			EFI_STATUS status = kBootServices->HandleProtocol(node->handle,
				&blockIoGuid, (void**)&blockIo);
			if (status != EFI_SUCCESS) {
				dprintf("unable to get block IO for device path\n");
				continue;
			}
			if (!blockIo->Media->MediaPresent) {
				dprintf("media not present for device path\n");
				continue;
			}

			EfiDevice *device = new(std::nothrow)EfiDevice(blockIo, node->device_path);
			if (device == NULL)
				continue;

			devicesList->Insert(device);
			dprintf("added a new messaging device to devices list\n");
		} else {
			dprintf("skipping existing messaging device path\n");
		}
	}

	return B_OK;
}
#endif

#if 0
static bool
get_boot_uuid(void)
{
	return false;
}
#endif

status_t
platform_add_boot_device(struct stage2_args *args, NodeList *devicesList)
{
#if 0
	dprintf("platform_add_boot_device()\n");
	// This is the first entry point, so init the lists here
	list_init(&sMessagingDevices);
	list_init(&sMediaDevices);
	dprintf("initialised list structures\n");

	build_device_handles();
	dprintf("built device handle lists\n");
	build_device_handles();
	dprintf("run a second time, nothing should be added\n");

	if (get_boot_uuid()) {
		// If we have the UUID, add the boot device containing that partition
		return add_boot_device(devicesList);
	} else {
		// If we don't have a UUID, add all CD devices with media
		dprintf("adding CD devices\n");
		return add_cd_devices(devicesList);
	}

	// Otherwise, we don't know what the boot device is; defer to
	// platform_add_block_devices() to add the rest
	return B_ENTRY_NOT_FOUND;
#else
	EFI_LOADED_IMAGE *loadedImage;
	if  (kBootServices->HandleProtocol(kImage, &LoadedImageGUID,
			(void**)&loadedImage) != EFI_SUCCESS)
		return B_ERROR;

	EFI_DEVICE_PATH *devicePath, *node;
	if (kBootServices->HandleProtocol(loadedImage->DeviceHandle,
			&DevicePathGUID, (void **)&devicePath) != EFI_SUCCESS)
		panic("Failed to lookup boot device path!");

	for (node = devicePath;DevicePathType(node) != MESSAGING_DEVICE_PATH;
			node = NextDevicePathNode(node)) {
		if (IsDevicePathEnd(node))
			panic("Could not find disk of EFI partition!");
	}

	SetDevicePathEndNode(NextDevicePathNode(node));

	EFI_HANDLE handle;
	if (kBootServices->LocateDevicePath(&BlockIoGUID, &devicePath, &handle)
			!= EFI_SUCCESS)
		panic("Cannot get boot device handle!");

	EFI_BLOCK_IO *blockIo;
	if (kBootServices->HandleProtocol(handle, &BlockIoGUID, (void**)&blockIo)
			!= EFI_SUCCESS)
		panic("Cannot get boot block io protocol!");

	if (!blockIo->Media->MediaPresent)
		panic("Boot disk has no media present!");


	EfiDevice *device = new(std::nothrow)EfiDevice(blockIo, devicePath);
	if (device == NULL)
		panic("Can't allocate memory for boot device!");

	devicesList->Insert(device);
	return B_OK;
#endif
}


status_t
platform_add_block_devices(struct stage2_args *args, NodeList *devicesList)
{
#if 0
	// Add all other devices not yet added
	dprintf("platform_add_block_devices()\n");
	return add_remaining_devices(devicesList);
#else
	EFI_BLOCK_IO *blockIo;
	UINTN memSize = 0;

	// Read to zero sized buffer to get memory needed for handles
	if (kBootServices->LocateHandle(ByProtocol, &BlockIoGUID, 0, &memSize, 0)
			!= EFI_BUFFER_TOO_SMALL)
		panic("Cannot read size of block device handles!");

	uint32 noOfHandles = memSize / sizeof(EFI_HANDLE);

	EFI_HANDLE handles[noOfHandles];
	if (kBootServices->LocateHandle(ByProtocol, &BlockIoGUID, 0, &memSize,
			handles) != EFI_SUCCESS)
		panic("Failed to locate block devices!");

	for (uint32 n = 0; n < noOfHandles; n++) {
		if (kBootServices->HandleProtocol(handles[n], &BlockIoGUID,
				(void**)&blockIo) != EFI_SUCCESS)
			panic("Cannot get block device handle!");

		// Skip partition block devices, Haiku does partition scan
		if (!blockIo->Media->MediaPresent || blockIo->Media->LogicalPartition)
			continue;

		EFI_DEVICE_PATH *devicePath;
		// Atm devicePath isn't necessary so result isn't checked
		kBootServices->HandleProtocol(handles[n], &DevicePathGUID,
			(void**)&devicePath);

		EfiDevice *device = new(std::nothrow)EfiDevice(blockIo, devicePath);
		if (device == NULL)
			panic("Can't allocate memory for block devices!");
		devicesList->Insert(device);
	}
	return devicesList->Count() > 0 ? B_OK : B_ENTRY_NOT_FOUND;
#endif
}


status_t
platform_get_boot_partition(struct stage2_args *args, Node *bootDevice,
		NodeList *partitions, boot::Partition **_partition)
{
	dprintf("platform_get_boot_partition()\n");
	NodeIterator iterator = partitions->GetIterator();
	boot::Partition *partition = NULL;
	while ((partition = (boot::Partition *)iterator.Next()) != NULL) {
		//Currently we pick first partition. If not found menu lets user select.
		*_partition = partition;
		return B_OK;
	}
	return B_ENTRY_NOT_FOUND;
}


status_t
platform_register_boot_device(Node *device)
{
	dprintf("platform_register_boot_device()\n");
	EfiDevice *efiDevice = (EfiDevice *)device;
	disk_identifier identifier;

	// TODO: Setup using device path
	identifier.bus_type = UNKNOWN_BUS;
	identifier.device_type = UNKNOWN_DEVICE;
	identifier.device.unknown.size = device->Size();

	for (uint32 i = 0; i < NUM_DISK_CHECK_SUMS; ++i) {
		off_t offset = get_next_check_sum_offset(i, device->Size());
		identifier.device.unknown.check_sums[i].offset = offset;
		identifier.device.unknown.check_sums[i].sum = compute_check_sum(device, offset);
	}

	gBootVolume.SetInt32(BOOT_METHOD, efiDevice->BootMethod());
	gBootVolume.SetBool(BOOT_VOLUME_BOOTED_FROM_IMAGE, efiDevice->ReadOnly());
	gBootVolume.SetData(BOOT_VOLUME_DISK_IDENTIFIER, B_RAW_TYPE,
		&identifier, sizeof(disk_identifier));

	return B_OK;
}
