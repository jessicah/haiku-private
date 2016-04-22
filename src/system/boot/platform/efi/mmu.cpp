#include "efi_platform.h"

#include <boot/platform.h>
#include <boot/stage2.h>
#include <kernel.h>


extern "C" status_t
platform_allocate_region(void **_virtualAddress, size_t size, uint8 protection,
	bool exactAddress)
{
	// called from loader/heap.cpp:
	//platform_allocate_region(&fAddress, fSize,
	//		B_READ_AREA | B_WRITE_AREA, false);
	EFI_PHYSICAL_ADDRESS addr;
	size_t aligned_size = ROUNDUP(size, B_PAGE_SIZE);

	EFI_STATUS status = kBootServices->AllocatePages(AllocateAnyPages,
		EfiLoaderData, aligned_size / B_PAGE_SIZE, &addr);
	if (status != EFI_SUCCESS)
		return B_NO_MEMORY;

	*_virtualAddress = (void*)addr;

	return B_OK;
}


extern "C" status_t
platform_free_region(void *address, size_t size)
{
	kBootServices->FreePages((EFI_PHYSICAL_ADDRESS)address,
		ROUNDUP(size, B_PAGE_SIZE) / B_PAGE_SIZE);

	return B_OK;
}
