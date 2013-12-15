/*
 * Copyright 2004-2008, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 */


#include "efi_platform.h"
#include "efiser.h"
#include "serial.h"

#include <boot/platform.h>
#include <arch/cpu.h>
#include <boot/stage2.h>

#include <string.h>


//#define ENABLE_SERIAL
	// define this to always enable serial output


static EFI_GUID sSerialIOProtocolGUID = SERIAL_IO_PROTOCOL;
static const uint32 kSerialBaudRate = 115200;

static SERIAL_IO_INTERFACE *sSerial = NULL;
static int32 sSerialEnabled = 0;


static void
serial_putc(char c)
{
	UINTN bufSize = 1;
	/* The only real EFI example I've seen (grub) doesn't check if it is ok to
		send so I assuem we don't need to check. Seems to work. */
	sSerial->Write(sSerial, &bufSize, &c);
}


extern "C" void
serial_puts(const char* string, size_t size)
{
	if (sSerialEnabled <= 0 || sSerial == NULL)
		return;

	//TODO: We can write strings instead of char by char.
	while (size-- != 0) {
		char c = string[0];

		if (c == '\n') {
			serial_putc('\r');
			serial_putc('\n');
		} else if (c != '\r')
			serial_putc(c);

		string++;
	}
}


extern "C" void
serial_disable(void)
{
#ifdef ENABLE_SERIAL
	sSerialEnabled = 0;
#else
	sSerialEnabled--;
#endif
}


extern "C" void
serial_enable(void)
{
	sSerialEnabled++;
}


extern "C" void
serial_init(void)
{
	//Grab the first serial, we could grab handles and iterate if we want all..
	EFI_STATUS status = kSystemTable->BootServices->LocateProtocol(
		&sSerialIOProtocolGUID, NULL, (void**)&sSerial);

	if (EFI_ERROR(status) || sSerial == NULL) {
		sSerial = NULL;
		return;
	}

	// Setup serial, 0, 0 = Default Receive FIFO queue and default timeout
	status = sSerial->SetAttributes(sSerial, kSerialBaudRate, 0, 0, NoParity, 8,
		OneStopBit);

	if (EFI_ERROR(status)) {
		sSerial = NULL;
		return;
	}

/* TODO, this can probably be skipped.
	// copy the base ports of the optional 4 serial ports to the kernel args
	// 0x0000:0x0400 is the location of that information in the BIOS data
	// segment

	uint16* ports = (uint16*)0x400;
	memcpy(gKernelArgs.platform_args.serial_base_ports, ports,
		sizeof(uint16) * MAX_SERIAL_PORTS);

	// only use the port if we could find one, else use the standard port
	if (gKernelArgs.platform_args.serial_base_ports[0] != 0)
		sSerialBasePort = gKernelArgs.platform_args.serial_base_ports[0];
*/

#ifdef ENABLE_SERIAL
	serial_enable();
#endif
}
