#include <string.h>

#include <boot/platform.h>
#include <boot/stage2.h>
#include <boot/stdio.h>

#include "efi_platform.h"


void
dprintf_args(const char *format, va_list args)
{
	vprintf(format, args);
}


extern "C" void
dprintf(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	dprintf_args(format, args);
	va_end(args);
}


extern "C" void
panic(const char *format, ...)
{
	va_list args;

	platform_switch_to_text_mode();

	puts("*** PANIC ***");

	va_start(args, format);
	vprintf(format, args);
	va_end(args);

	while (true)
		kBootServices->Stall(1000000);
}


char*
platform_debug_get_log_buffer(size_t *_size)
{
	return NULL;
}
