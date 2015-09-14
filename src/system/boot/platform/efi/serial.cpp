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
static bool sSerialUsesEFI = true;


enum serial_register_offsets {
	SERIAL_TRANSMIT_BUFFER		= 0,
	SERIAL_RECEIVE_BUFFER		= 0,
	SERIAL_DIVISOR_LATCH_LOW	= 0,
	SERIAL_DIVISOR_LATCH_HIGH	= 1,
	SERIAL_FIFO_CONTROL			= 2,
	SERIAL_LINE_CONTROL			= 3,
	SERIAL_MODEM_CONTROL		= 4,
	SERIAL_LINE_STATUS			= 5,
	SERIAL_MODEM_STATUS			= 6,
};

static uint16 sSerialBasePort = 0x3f8;


static void
serial_putc(char c)
{
	UINTN bufSize = 1;
	/* The only real EFI example I've seen (grub) doesn't check if it is ok to
		send so I assuem we don't need to check. Seems to work. */
	if (sSerialUsesEFI) {
		sSerial->Write(sSerial, &bufSize, &c);
	} else {
		while ((in8(sSerialBasePort + SERIAL_LINE_STATUS) & 0x20) == 0)
			asm volatile ("pause;");

		out8(c, sSerialBasePort + SERIAL_TRANSMIT_BUFFER);
	}
}


extern "C" void
serial_puts(const char* string, size_t size)
{
	if (sSerialEnabled <= 0 || (sSerial == NULL && sSerialUsesEFI))
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

	// just don't use EFI at all...? :p
	serial_switch_to_bios();
}


extern "C" void
serial_switch_to_bios(void)
{
	// we can't use this once we exit UEFI
	sSerial = NULL;
	sSerialUsesEFI = false;

	// copy the base ports of the optional 4 serial ports to the kernel args
	// 0x0000:0x0400 is the location of that information in the BIOS data
	// segment
	uint16_t* ports = (uint16_t*)0x400;
	memcpy(gKernelArgs.platform_args.serial_base_ports, ports,
		sizeof(uint16) * MAX_SERIAL_PORTS);

	// only use the port if we could find one, else use the standard port
	if (gKernelArgs.platform_args.serial_base_ports[0] != 0)
		sSerialBasePort = gKernelArgs.platform_args.serial_base_ports[0];

	uint16 divisor = uint16(115200 / kSerialBaudRate);

	out8(0x80, sSerialBasePort + SERIAL_LINE_CONTROL);
		// set divisor latch access bit
	out8(divisor & 0xf, sSerialBasePort + SERIAL_DIVISOR_LATCH_LOW);
	out8(divisor >> 8, sSerialBasePort + SERIAL_DIVISOR_LATCH_HIGH);
	out8(3, sSerialBasePort + SERIAL_LINE_CONTROL);
		// 8N1
}
