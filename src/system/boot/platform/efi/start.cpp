/*
 * Copyright 2013 - 2014, Fredrik Holmqvist, fredrik.holmqvist@gmail.com.
 * All rights reserved.
 * Distributed under the terms of the Haiku License.
 */


#include <string.h>

#include <KernelExport.h>

#include <arch/cpu.h>
#include <boot/platform.h>
#include <boot/heap.h>
#include <boot/stage2.h>

//#include "acpi.h"
//#include "apm.h"
//#include "bios.h"
#include "console.h"
//#include "cpu.h"
#include "debug.h"
//#include "hpet.h"
//#include "interrupts.h"
#include "keyboard.h"
//#include "long.h"
//#include "multiboot.h"
#include "serial.h"
//#include "smp.h"


extern void (*__ctor_list)(void);
extern void (*__ctor_end)(void);


const EFI_SYSTEM_TABLE		*kSystemTable;
const EFI_BOOT_SERVICES		*kBootServices;
const EFI_RUNTIME_SERVICES	*kRuntimeServices;


static uint32 sBootOptions;


extern "C" int main(stage2_args *args);
extern "C" void _start(void);


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


extern "C" void
platform_start_kernel(void)
{
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

	memset(&args, 0, sizeof(stage2_args));

	kSystemTable = systemTable;
	kBootServices = systemTable->BootServices;
	kRuntimeServices = systemTable->RuntimeServices;

	asm("cld");			// Ain't nothing but a GCC thang.
	asm("fninit");		// initialize floating point unit

	/* Needed, Intel Beyond BIOS http://software.intel.com/en-us/articles/uefi-application
	 * No direct support
	 * New and Delete can be mapped to malloc/free
	 */
	call_ctors();
		// call C++ constructors before doing anything else

  //Do all the necessary things needed...

	serial_init();
	serial_enable();
//	interrupts_init();
	console_init();
//	cpu_init();
//	mmu_init();
	debug_init_post_mmu();

  /* parse_multiboot_commandline - probably not */
  /* check for boot keys */

  /* APM - skip entirely */
  /* ACPI - do we need to mmap/checksum it ? Or does EFI help with that.. Is it needed */
  /* smp - TODO */
  /* HPET - TODO */
  /* dump_multiboot_info */


  /* Map runtime services to virtual memory
   * Get/Set EFI Variables
   * Get/Set time (with timezone)
   * Get/Set Wakup time
   * Reset system
   * Send 'capsules'? to EFI to be executed
   */
  /* Exit boot services - Only use runtime ( and maybe some system services) from here on.
   * See info on memory map and much more in docs. */

	main(&args);

	return EFI_SUCCESS;
}
