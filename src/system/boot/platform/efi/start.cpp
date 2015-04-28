/*
 * Copyright 2013 - 2014, Fredrik Holmqvist, fredrik.holmqvist@gmail.com.
 * Copyright 2014, Henry Harrington, henry.harrington@gmail.com.
 * All rights reserved.
 * Distributed under the terms of the Haiku License.
 */


#include <string.h>

#include <KernelExport.h>

#include <arch/cpu.h>
#include <arch/x86/descriptors.h>
#include <boot/platform.h>
#include <boot/heap.h>
#include <boot/stage2.h>
#include <boot/kernel_args.h>
#include <kernel.h>

#include "acpi.h"
//#include "apm.h"
//#include "bios.h"
#include "console.h"
//#include "cpu.h"
#include "debug.h"
#include "hpet.h"
//#include "interrupts.h"
#include "keyboard.h"
//#include "multiboot.h"
#include "serial.h"
#include "smp.h"
#include "mmu.h"


extern void (*__ctor_list)(void);
extern void (*__ctor_end)(void);


const EFI_SYSTEM_TABLE		*kSystemTable;
const EFI_BOOT_SERVICES		*kBootServices;
const EFI_RUNTIME_SERVICES	*kRuntimeServices;
EFI_HANDLE kImage;

static uint32 sBootOptions;
static uint64 gLongKernelEntry;
extern uint64 gLongGDT;
segment_descriptor gBootGDT[BOOT_GDT_SEGMENT_COUNT];


extern "C" int main(stage2_args *args);
extern "C" void _start(void);
extern "C" void efi_enter_kernel(uint64 pml4, uint64 entry_point, uint64 stack);


static void
call_ctors(void)
{
	void (**f)(void);

	for (f = &__ctor_list; f < &__ctor_end; f++) {
		(**f)();
	}
}


extern "C" uint32
platform_boot_options(void)
{
	return (sBootOptions | BOOT_OPTION_MENU);
}

extern "C" status_t
platform_bootloader_address_to_kernel_address(void *address, uint64_t *_result);

/*! Convert a 32-bit address to a 64-bit address. */
static inline uint64
fix_address(uint64 address)
{
	uint64 result;
	if (platform_bootloader_address_to_kernel_address((void *)address, &result) != B_OK)
		return address;
	else
		return result;
}

template<typename Type>
inline void
fix_address(FixedWidthPointer<Type>& p)
{
	if (p != NULL)
		p.SetTo(fix_address(p.Get()));
}

static void
long_gdt_init()
{
	clear_segment_descriptor(&gBootGDT[0]);

	// Set up code/data segments (TSS segments set up later in the kernel).
	set_segment_descriptor(&gBootGDT[KERNEL_CODE_SEGMENT], DT_CODE_EXECUTE_ONLY,
		DPL_KERNEL);
	set_segment_descriptor(&gBootGDT[KERNEL_DATA_SEGMENT], DT_DATA_WRITEABLE,
		DPL_KERNEL);
	set_segment_descriptor(&gBootGDT[USER_CODE_SEGMENT], DT_CODE_EXECUTE_ONLY,
		DPL_USER);
	set_segment_descriptor(&gBootGDT[USER_DATA_SEGMENT], DT_DATA_WRITEABLE,
		DPL_USER);

	// Used by long_enter_kernel().
	gLongGDT = (addr_t)gBootGDT + 0xFFFFFF0000000000;
	dprintf("GDT at 0x%lx\n", gLongGDT);
}

static void
convert_preloaded_image(preloaded_elf64_image* image)
{
	fix_address(image->next);
	fix_address(image->name);
	fix_address(image->debug_string_table);
	fix_address(image->syms);
	fix_address(image->rel);
	fix_address(image->rela);
	fix_address(image->pltrel);
	fix_address(image->debug_symbols);
}

/*!	Convert all addresses in kernel_args to 64-bit addresses. */
static void
convert_kernel_args()
{
	fix_address(gKernelArgs.boot_volume);
	fix_address(gKernelArgs.vesa_modes);
	fix_address(gKernelArgs.edid_info);
	fix_address(gKernelArgs.debug_output);
	fix_address(gKernelArgs.boot_splash);
	fix_address(gKernelArgs.arch_args.apic);
	fix_address(gKernelArgs.arch_args.hpet);

	convert_preloaded_image(static_cast<preloaded_elf64_image*>(
		gKernelArgs.kernel_image.Pointer()));
	fix_address(gKernelArgs.kernel_image);

	// Iterate over the preloaded images. Must save the next address before
	// converting, as the next pointer will be converted.
	preloaded_image* image = gKernelArgs.preloaded_images;
	fix_address(gKernelArgs.preloaded_images);
	dprintf("preloaded images:\n");
	while (image != NULL) {
		dprintf("image: id = %d, name = %s, next = %p\n", image->id, image->name.Pointer(), image->next.Pointer());
		preloaded_image* next = image->next;
		convert_preloaded_image(static_cast<preloaded_elf64_image*>(image));
		image = next;
	}

	// Fix driver settings files.
	driver_settings_file* file = gKernelArgs.driver_settings;
	fix_address(gKernelArgs.driver_settings);
	while (file != NULL) {
		driver_settings_file* next = file->next;
		fix_address(file->next);
		fix_address(file->buffer);
		file = next;
	}
}

extern "C" void
platform_start_kernel(void)
{
	if (gKernelArgs.kernel_image->elf_class != ELFCLASS64) {
		panic("32-bit kernels not supported with EFI");
	}

	preloaded_elf64_image *image = static_cast<preloaded_elf64_image *>(
		gKernelArgs.kernel_image.Pointer());

	acpi_init();
	smp_init();
	smp_init_other_cpus();
	hpet_init();

	// TODO: all these things.
	gKernelArgs.arch_args.system_time_cv_factor = 1234;
	gKernelArgs.arch_args.cpu_clock_speed = 666;
	gKernelArgs.arch_args.apic_time_cv_factor = 42;
	gKernelArgs.debug_size = 0;

	long_gdt_init();
	//long_mmu_init();
	debug_cleanup();
	convert_kernel_args();

	// Save the kernel entry point address.
	gLongKernelEntry = image->elf_header.e_entry;
	dprintf("kernel entry at %#lx\n", gLongKernelEntry);

	// map in a kernel stack
	void *stack_address = NULL;
	if (platform_allocate_region(&stack_address, KERNEL_STACK_SIZE + KERNEL_STACK_GUARD_PAGES * B_PAGE_SIZE, 0, false) != B_OK) {
		panic("Unabled to allocate a stack");
	}
	gKernelArgs.cpu_kstack[0].start = fix_address((uint64_t)stack_address);
	gKernelArgs.cpu_kstack[0].size = KERNEL_STACK_SIZE + KERNEL_STACK_GUARD_PAGES * B_PAGE_SIZE;
	dprintf("Kernel stack at %#lx\n", gKernelArgs.cpu_kstack[0].start);

	// Prepare to exit EFI boot services.
	// Read the memory map.
	// First call is to determine the buffer size.
	UINTN memory_map_size = 0;
	EFI_MEMORY_DESCRIPTOR dummy;
	EFI_MEMORY_DESCRIPTOR *memory_map;
	UINTN map_key;
	UINTN descriptor_size;
	UINT32 descriptor_version;
	if (kBootServices->GetMemoryMap(&memory_map_size, &dummy, &map_key, &descriptor_size, &descriptor_version) != EFI_BUFFER_TOO_SMALL) {
		panic("Unable to determine size of system memory map");
	}

	// Allocate a buffer twice as large as needed just in case it gets bigger between
	// calls to ExitBootServices.
	UINTN actual_memory_map_size = memory_map_size * 2;
	memory_map = (EFI_MEMORY_DESCRIPTOR *)malloc(actual_memory_map_size);
	if (!memory_map) {
		panic("Unable to allocate memory map.");
	}

	// Read (and print) the memory map.
	memory_map_size = actual_memory_map_size;
	if (kBootServices->GetMemoryMap(&memory_map_size, memory_map, &map_key, &descriptor_size, &descriptor_version) != EFI_SUCCESS) {
		panic("Unable to fetch system memory map.");
	}

	addr_t addr = (addr_t)memory_map;
	dprintf("System provided memory map:\n");
	for (UINTN i = 0; i < memory_map_size / descriptor_size; ++i) {
		EFI_MEMORY_DESCRIPTOR *entry = (EFI_MEMORY_DESCRIPTOR *)(addr + i * descriptor_size);
		dprintf("  %#lx-%#lx  %#lx %#x %#lx\n",
			entry->PhysicalStart, entry->PhysicalStart + entry->NumberOfPages * 4096,
			entry->VirtualStart, entry->Type, entry->Attribute);
	}

	// Generate page tables for use after ExitBootServices.
	uint64_t final_pml4 = mmu_generate_post_efi_page_tables(memory_map_size, memory_map, descriptor_size, descriptor_version);
	dprintf("Final PML4 at %#lx\n", final_pml4);

	dprintf("sizeof platform_args = %lu\n", sizeof(platform_kernel_args));
	dprintf("sizeof arch args = %lu\n", sizeof(arch_kernel_args));

	// Attempt to fetch the memory map and exit boot services.
	// This needs to be done in a loop, as ExitBootServices can change the memory map.
	// Even better: Only GetMemoryMap and ExitBootServices can be called after the
	// first call to ExitBootServices, as the firmware is permitted to partially
	// exit. This is why twice as much space was allocated for the memory map,
	// as it's impossible to allocate more now.
	// A changing memory map shouldn't affect the generated page tables, as
	// they only needed to know about the maximum address, not any specific entry.
	dprintf("Calling ExitBootServices. So long, EFI!\n");
	while (1) {
		if (kBootServices->ExitBootServices(kImage, map_key) == EFI_SUCCESS) {
			break;
		}

		memory_map_size = actual_memory_map_size;
		if (kBootServices->GetMemoryMap(&memory_map_size, memory_map, &map_key, &descriptor_size, &descriptor_version) != EFI_SUCCESS) {
			panic("Unable to fetch system memory map.");
		}
	}
	// We're on our own now...

	// The console was provided by boot services, disable it.
	stdout = NULL;

	// Update EFI, generate final kernel physical memory map, etc.
	mmu_post_efi_setup(memory_map_size, memory_map, descriptor_size, descriptor_version);

	//smp_boot_other_cpus(long_smp_start_kernel);

	// Enter the kernel!
	efi_enter_kernel(final_pml4,
			 gLongKernelEntry,
			 gKernelArgs.cpu_kstack[0].start + gKernelArgs.cpu_kstack[0].size);

	panic("Shouldn't get here");
}


extern "C" void
platform_exit(void)
{
	kRuntimeServices->ResetSystem(EfiResetCold, 0, 0, NULL);
}


/**
 * efi_main - The entry point for the EFI application
 * @image: firmware-allocated handle that identifies the image
 * @SystemTable: EFI system table
 */
extern "C" EFI_STATUS
efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systemTable)
{
	stage2_args args;

	kImage = image;
	kSystemTable = systemTable;
	kBootServices = systemTable->BootServices;
	kRuntimeServices = systemTable->RuntimeServices;

	asm("cld");			// Ain't nothing but a GCC thang.
	asm("fninit");		// initialize floating point unit

	memset(&args, 0, sizeof(stage2_args));

	/* Needed, Intel Beyond BIOS http://software.intel.com/en-us/articles/uefi-application
	 * No direct support
	 * New and Delete can be mapped to malloc/free
	 */
	call_ctors();
		// call C++ constructors before doing anything else

	// Do all the necessary things needed...

	// Anything that depends on allocation needs to go
	// into platform_start_kernel instead.
	// Anything that currently uses mmu_map_physical_memory,
	// for example, needs to go there instead.

	serial_init();
	serial_enable();
//	interrupts_init();
	console_init();
//	cpu_init();
//	mmu_init();
	debug_init_post_mmu();

	// Disable apm in case we ever load a 32-bit kernel...
	gKernelArgs.platform_args.apm.version = 0;

	/* Map runtime services to virtual memory
	 * Get/Set EFI Variables
	 * Get/Set time (with timezone)
	 * Get/Set Wakup time
	 * Reset system
	 * Send 'capsules'? to EFI to be executed
	 */

	main(&args);

	return EFI_SUCCESS;
}
