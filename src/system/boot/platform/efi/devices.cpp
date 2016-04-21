#include <boot/platform.h>
#include <boot/stage2.h>


status_t
platform_add_boot_device(struct stage2_args *args, NodeList *devicesList)
{
	return B_ERROR;
}


status_t
platform_add_block_devices(struct stage2_args *args, NodeList *devicesList)
{
	return B_ERROR;
}


status_t
platform_get_boot_partition(struct stage2_args *args, Node *bootDevice,
		NodeList *partitions, boot::Partition **_partition)
{
	return B_ERROR;
}


status_t
platform_register_boot_device(Node *device)
{
	return B_ERROR;
}
