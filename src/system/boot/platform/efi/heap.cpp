/*
 * Copyright 2014, Jessica Hamilton, jessica.l.hamilton@gmail.com.
 * Distributed under the terms of the MIT License.
 */


#include "efi_platform.h"

#include <boot/platform.h>
#include <boot/stage2.h>


#define STAGE_PAGES	0x2000	/* 32 MB */
#define PAGE_SIZE	0x1000	/*  4 kB */


EFI_PHYSICAL_ADDRESS staging;


void
platform_release_heap(struct stage2_args *args, void *base)
{
	ASSERT((void *)staging == base);
	kBootServices->FreePages(staging, STAGE_PAGES);
}


status_t
platform_init_heap(struct stage2_args *args, void **_base, void **_top)
{
	if (kBootServices->AllocatePages(AllocateAnyPages, EfiLoaderData, STAGE_PAGES, &staging) != EFI_SUCCESS)
		return B_NO_MEMORY;

	*_base = (void *)staging;
	*_top = (void *)((int8 *)staging + STAGE_PAGES * PAGE_SIZE);

	return B_OK;
}
