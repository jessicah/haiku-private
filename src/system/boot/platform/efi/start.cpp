/*
 * Copyright 2013, Fredrik Holmqvist, fredrik.holmqvist@gmail.com. All rights reserved.
 * Distributed under the terms of the Haiku License.
 */


#include <string.h>

#include <KernelExport.h>

#include <arch/cpu.h>
#include <boot/platform.h>
#include <boot/heap.h>
#include <boot/stage2.h>
#include <stdio.h>

#include "efi_platform.h"

#include "console.h"
#include "interrupts.h"


extern void (*__ctor_list)(void);
extern void (*__ctor_end)(void);
extern uint8 __bss_start;
extern uint8 _end;


const EFI_SYSTEM_TABLE *kSystemTable;


static void
clear_bss(void)
{
	memset(&__bss_start, 0, &_end - &__bss_start);
}


static void
call_ctors(void)
{
	void (**f)(void);

	for (f = &__ctor_list; f < &__ctor_end; f++) {
		(**f)();
	}
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

	kSystemTable = systemTable;
	asm("cld");			// Ain't nothing but a GCC thang.
	asm("fninit");		// initialize floating point unit

	/* Needed, Intel Beyond BIOS http://software.intel.com/en-us/articles/uefi-application
	 * No direct support
	 * New and Delete can be mapped to malloc/free
	 */
	clear_bss();
	call_ctors();
		// call C++ constructors before doing anything else

  //Do all the necessary things needed...

  /* serial - ignore for now */
	/* interrupts_init(); */
	console_init();
  /* console - EFI console handling */
  /* cpu init - TODO */
  /* mmu init - TODO */
  /* debug_init_post_mmu - TODO */
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

  //launch kernel (main(&args);)

/*
	UINTN index;
	EFI_EVENT event = systemTable->ConIn->WaitForKey;

	SIMPLE_INPUT_INTERFACE *conIn = systemTable->ConIn;
	SIMPLE_TEXT_OUTPUT_INTERFACE *conOut = systemTable->ConOut;
	conOut->OutputString(conOut, exampleText);

	systemTable->BootServices->WaitForEvent(1, &event, &index);
*/
	console_wait_for_key();
	return EFI_SUCCESS;
}
