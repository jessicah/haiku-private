/*
 * Copyright 2014, Jessica Hamilton, jessica.l.hamilton@gmail.com.
 * Distributed under the terms of the MIT License.
 */


#include "efi_platform.h"

#include <boot/platform.h>
#include <boot/stage2.h>


// As far as I know, EFI uses identity mapped memory, and we already have paging enabled


static addr_t
mmu_map_physical_memory(addr_t physicalAddress, size_t size)
{
	return physicalAddress;
}

static void
mmu_free(void *virtualAddress, size_t size)
{
}


extern "C" status_t
platform_allocate_region(void **_address, size_t size, uint8 /* protection */, bool /* exactAddress */)
{
	if (kBootServices->AllocatePool(EfiLoaderData, size, _address) != EFI_SUCCESS)
		return B_NO_MEMORY;

	return B_OK;
}


extern "C" status_t
platform_free_region(void *address, size_t /* size */)
{
	kBootServices->FreePool(address);

	return B_OK;
}
