/*
 * Copyright 2005-2012, Axel Dörfler, axeld@pinc-software.de.
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "accelerant_protos.h"
#include "accelerant.h"

#include <new>

#define TRACE_HOOKS
#ifdef TRACE_HOOKS
extern "C" void _sPrintf(const char *format, ...);
#	define TRACE(x) _sPrintf x
#else
#	define TRACE(x) ;
#endif


extern "C" void*
get_accelerant_hook(uint32 feature, void* data)
{
	switch (feature) {
		/* general */
		case B_INIT_ACCELERANT:
			TRACE(("\33[36mframebuffer.accelerant:\33[0m INIT_ACCELERANT hook\n"));
			return (void*)framebuffer_init_accelerant;
		case B_UNINIT_ACCELERANT:
			return (void*)framebuffer_uninit_accelerant;
		case B_CLONE_ACCELERANT:
			return (void*)framebuffer_clone_accelerant;
		case B_ACCELERANT_CLONE_INFO_SIZE:
			return (void*)framebuffer_accelerant_clone_info_size;
		case B_GET_ACCELERANT_CLONE_INFO:
			return (void*)framebuffer_get_accelerant_clone_info;
		case B_GET_ACCELERANT_DEVICE_INFO:
			return (void*)framebuffer_get_accelerant_device_info;
		case B_ACCELERANT_RETRACE_SEMAPHORE:
			return (void*)framebuffer_accelerant_retrace_semaphore;

		/* mode configuration */
		case B_ACCELERANT_MODE_COUNT:
			return (void*)framebuffer_accelerant_mode_count;
		case B_GET_MODE_LIST:
			return (void*)framebuffer_get_mode_list;
		case B_PROPOSE_DISPLAY_MODE:
			TRACE(("\33[36mframebuffer.accelerant:\33[0 mPROPROSE_DISPLAY_MODE\n"));
			return NULL;
		case B_SET_DISPLAY_MODE:
			return (void*)framebuffer_set_display_mode;
		case B_GET_DISPLAY_MODE:
			return (void*)framebuffer_get_display_mode;
		case B_GET_EDID_INFO:
			TRACE(("\33[36mframebuffer.accelerant:\33[0m GET_EDID_INFO\n"));
			return NULL;
		case B_GET_FRAME_BUFFER_CONFIG:
			return (void*)framebuffer_get_frame_buffer_config;
		case B_GET_PIXEL_CLOCK_LIMITS:
			return (void*)framebuffer_get_pixel_clock_limits;
		case B_MOVE_DISPLAY:
			TRACE(("\33[36mframebuffer.accelerant:\33[0m MOVE_DISPLAY\n"));
			return NULL;
		case B_SET_INDEXED_COLORS:
			TRACE(("\33[36mframebuffer.accelerant:\33[0m SET_INDEXED_COLORS\n"));
			return NULL;
		case B_GET_TIMING_CONSTRAINTS:
			TRACE(("\33[36mframebuffer.accelerant:\33[0m SET_TIMING_CONSTRAINTS\n"));
			return NULL;

		/* DPMS */
		case B_DPMS_CAPABILITIES:
			TRACE(("\33[36mframebuffer.accelerant:\33[0m DPMS_CAPABILITIES\n"));
			return NULL;
		case B_DPMS_MODE:
			TRACE(("\33[36mframebuffer.accelerant:\33[0m DPMS_MODE\n"));
			return NULL;
		case B_SET_DPMS_MODE:
			TRACE(("\33[36mframebuffer.accelerant:\33[0m SET_DPMS_MODE\n"));
			return NULL;

		/* engine/synchronization */
		case B_ACCELERANT_ENGINE_COUNT:
			return (void*)framebuffer_accelerant_engine_count;
		case B_ACQUIRE_ENGINE:
			return (void*)framebuffer_acquire_engine;
		case B_RELEASE_ENGINE:
			return (void*)framebuffer_release_engine;
		case B_WAIT_ENGINE_IDLE:
			return (void*)framebuffer_wait_engine_idle;
		case B_GET_SYNC_TOKEN:
			return (void*)framebuffer_get_sync_token;
		case B_SYNC_TO_TOKEN:
			return (void*)framebuffer_sync_to_token;
	}

	TRACE(("\33[36mframebuffer.accelerant:\33[0m unknown hook! (%" B_PRIx32 ")",
		feature));
	return NULL;
}

