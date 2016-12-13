/*
 * Copyright 2005-2009, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 */
#ifndef VESA_PRIVATE_H
#define VESA_PRIVATE_H


#include <Drivers.h>
#include <Accelerant.h>


#define DEVICE_NAME				"\33[36mframebuffer\33[0m"
#define VESA_ACCELERANT_NAME	"\33[36mframebuffer.accelerant\33[0m"


struct vesa_get_supported_modes;
struct vesa_mode;

struct framebuffer_info {
	uint32			cookie_magic;
	int32			open_count;
	int32			id;
	struct vesa_shared_info* shared_info;
	area_id			shared_area;
	vesa_mode*		modes;

	addr_t			frame_buffer;
	addr_t			physical_frame_buffer;
	size_t			physical_frame_buffer_size;
	bool			complete_frame_buffer_mapped;
};

extern status_t framebuffer_init(framebuffer_info& info);
extern void framebuffer_uninit(framebuffer_info& info);


#endif	/* VESA_PRIVATE_H */
