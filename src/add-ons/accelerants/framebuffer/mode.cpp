/*
 * Copyright 2005-2015, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 */


#include <stdlib.h>
#include <string.h>

#include <compute_display_timing.h>
#include <create_display_modes.h>

#include "accelerant_protos.h"
#include "accelerant.h"
#include "utility.h"


#define TRACE_MODE
#ifdef TRACE_MODE
extern "C" void _sPrintf(const char* format, ...);
#	define TRACE(x) _sPrintf x
#else
#	define TRACE(x) ;
#endif


static void
dump_display_mode(const display_mode &mode)
{
	TRACE(("\33[36m    display mode:\n"
		"      \33[36mdimensions: \33[32m%dx%d\n"
		"      \33[36mdisplay start: \33[32m%d:%d\33[0m"
		"      \33[36mcolor space: \33[32m%x\33[0m\n",
		mode.virtual_width, mode.virtual_height,
		mode.h_display_start, mode.v_display_start,
		mode.space));
}


bool
operator==(const display_mode &lhs, const display_mode &rhs)
{
	TRACE(("comparing display modes\n"));
	TRACE(("  first display_mode:\n"));
	dump_display_mode(lhs);
	TRACE(("  second display_mode:\n"));
	dump_display_mode(rhs);

	return lhs.space == rhs.space
		&& lhs.virtual_width == rhs.virtual_width
		&& lhs.virtual_height == rhs.virtual_height
		&& lhs.h_display_start == rhs.h_display_start
		&& lhs.v_display_start == rhs.v_display_start;
}


static bool
is_mode_supported(display_mode *mode)
{
	// see if it matches the boot framebuffer, we don't support anything else
	if (mode != NULL && *mode == gInfo->shared_info->current_mode) {
		TRACE(("  result: found mode matching current\n"));
		return true;
	}

	TRACE(("  result: not a matching display mode\n"));
	return false;
}


status_t
create_mode_list(void)
{
	color_space colorspaces[] = {
		(color_space)gInfo->shared_info->current_mode.space
	};
	display_mode mode = gInfo->shared_info->current_mode;

	TRACE(("create_mode_list\n"));
	dump_display_mode(mode);
	compute_display_timing(mode.virtual_width, mode.virtual_height, 60, false,
		&mode.timing);
	fill_display_mode(mode.virtual_width, mode.virtual_height, &mode);

	gInfo->mode_list_area = create_display_modes("framebuffer modes",
		NULL, &mode, 1, colorspaces, 1, is_mode_supported, &gInfo->mode_list,
		&gInfo->shared_info->mode_count);

	TRACE(("created %d display modes\n", gInfo->shared_info->mode_count));

	if (gInfo->mode_list_area < 0) {
		TRACE(("FAILED TO CREATE DISPLAY MODES\n"));
		return gInfo->mode_list_area;
	}

	gInfo->shared_info->mode_list_area = gInfo->mode_list_area;
	return B_OK;
}


uint32
framebuffer_accelerant_mode_count(void)
{
	TRACE(("framebuffer_accelerant_mode_count() = %d\n", gInfo->shared_info->mode_count));
	return gInfo->shared_info->mode_count;
}


status_t
framebuffer_get_display_mode(display_mode* _currentMode)
{
	TRACE(("framebuffer_get_display_mode()\n"));
	dump_display_mode(gInfo->shared_info->current_mode);
	*_currentMode = gInfo->shared_info->current_mode;
	return B_OK;
}


status_t
framebuffer_get_frame_buffer_config(frame_buffer_config* config)
{
	TRACE(("framebuffer_get_frame_buffer_config()\n"));

	config->frame_buffer = gInfo->shared_info->frame_buffer;
	config->frame_buffer_dma = gInfo->shared_info->physical_frame_buffer;
	config->bytes_per_row = gInfo->shared_info->bytes_per_row;

	return B_OK;
}


status_t
framebuffer_get_mode_list(display_mode* modeList)
{
	TRACE(("framebuffer_get_mode_info()\n"));
	memcpy(modeList, gInfo->mode_list,
		gInfo->shared_info->mode_count * sizeof(display_mode));
	return B_OK;
}


status_t
framebuffer_set_display_mode(display_mode* _mode)
{
	TRACE(("framebuffer_set_display_mode()\n"));
	if (_mode != NULL && *_mode == gInfo->shared_info->current_mode) {
		TRACE(("    set_display_mode OK\n"));
		return B_OK;
	} else {
		TRACE(("  set_display_mode unsupported\n"));
	}

	return B_UNSUPPORTED;
}


status_t
framebuffer_get_pixel_clock_limits(display_mode* mode, uint32* _low, uint32* _high)
{
	TRACE(("framebuffer_get_pixel_clock_limits()\n"));

	// TODO: do some real stuff here (taken from radeon driver)
	uint32 totalPixel = (uint32)mode->timing.h_total
		* (uint32)mode->timing.v_total;
	uint32 clockLimit = 2000000;

	// lower limit of about 48Hz vertical refresh
	*_low = totalPixel * 48L / 1000L;
	if (*_low > clockLimit)
		return B_ERROR;

	*_high = clockLimit;
	return B_OK;
}
