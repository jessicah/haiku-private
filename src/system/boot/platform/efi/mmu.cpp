#include <boot/platform.h>
#include <boot/stage2.h>


extern "C" status_t
platform_allocate_region(void **_virtualAddress, size_t size, uint8 protection,
	bool exactAddress)
{
	return B_ERROR;
}


extern "C" status_t
platform_free_region(void *address, size_t size)
{
	return B_ERROR;
}
