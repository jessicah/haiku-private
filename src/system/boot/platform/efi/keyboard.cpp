/*
 * Copyright 2004-2011, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 */


#include "keyboard.h"


#include <boot/platform.h>
#include "efi_platform.h"

/*!	Note, checking for keys doesn't seem to work in graphics
	mode, at least in Bochs.
*/
static uint16
check_for_key(void)
{
	EFI_INPUT_KEY key;
	return (kSystemTable->ConIn->ReadKeyStroke(kSystemTable->ConIn, &key) ==
		EFI_NOT_READY) ? 0 : key.UnicodeChar;
}


extern "C" void
clear_key_buffer(void)
{
	kSystemTable->ConIn->Reset(kSystemTable->ConIn, false);
}


extern "C" EFI_INPUT_KEY
wait_for_key(void)
{
	UINTN index;
	EFI_INPUT_KEY key;
	EFI_EVENT event = kSystemTable->ConIn->WaitForKey;
	do {
		kSystemTable->BootServices->WaitForEvent(1, &event, &index);
	} while (kSystemTable->ConIn->ReadKeyStroke(kSystemTable->ConIn, &key)
			== EFI_NOT_READY);

	return key;
}


extern "C" uint32
check_for_boot_keys(void)
{
/*
	bios_regs regs;
	uint32 options = 0;
	uint32 keycode = 0;
	regs.eax = 0x0200;
	call_bios(0x16, &regs);
		// Read Keyboard flags. bit 0 LShift, bit 1 RShift
	if ((regs.eax & 0x03) != 0) {
		// LShift or RShift - option menu
		options |= BOOT_OPTION_MENU;
	} else {
		keycode = boot_key_in_keyboard_buffer();
		if (keycode == 0x3920) {
			// space - option menu
			options |= BOOT_OPTION_MENU;
		} else if (keycode == 0x011B) {
			// ESC - debug output
			options |= BOOT_OPTION_DEBUG_OUTPUT;
		}
	}
*/
//	dprintf("options = %ld\n", options);
//	return options;
	return 0;
}
