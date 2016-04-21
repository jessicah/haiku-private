/*
 * Copyright 2014-2016, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Jessica Hamilton, jessica.l.hamilton@gmail.com
 */


#include "efi_platform.h"

#include <boot/stage2.h>
#include <boot/platform.h>
#include <boot/kernel_args.h>
#include <boot/platform/generic/video.h>


static UINTN sScreenMode;
static EFI_GRAPHICS_OUTPUT_PROTOCOL *sGraphicsOutput;


extern "C" status_t
platform_init_video(void)
{
	EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	EFI_STATUS status;
	UINTN area = 0;

	status = kBootServices->LocateProtocol(&gopGuid, NULL,
		(void**)&sGraphicsOutput);
	if (status != EFI_SUCCESS || sGraphicsOutput == NULL)
		goto noFrameBuffer;

	for (UINTN mode = 0; mode < sGraphicsOutput->Mode->MaxMode; ++mode) {
		UINTN size;
		EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;

		status = sGraphicsOutput->QueryMode(sGraphicsOutput, mode, &size, &info);
		if (status == EFI_SUCCESS && info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor
			&& info->HorizontalResolution * info->VerticalResolution >= area) {
				area = info->HorizontalResolution * info->VerticalResolution;
				sScreenMode = mode;
		}
	}

	if (area == 0)
		goto noFrameBuffer;

	gKernelArgs.frame_buffer.enabled = true;
	gKernelArgs.frame_buffer.physical_buffer.start =
		sGraphicsOutput->Mode->FrameBufferBase;
	gKernelArgs.frame_buffer.physical_buffer.size =
		sGraphicsOutput->Mode->FrameBufferSize;
	gKernelArgs.frame_buffer.width =
		sGraphicsOutput->Mode->Info->HorizontalResolution;
	gKernelArgs.frame_buffer.height =
		sGraphicsOutput->Mode->Info->VerticalResolution;
	gKernelArgs.frame_buffer.bytes_per_row =
		sGraphicsOutput->Mode->Info->PixelsPerScanLine * 4;
	gKernelArgs.frame_buffer.depth = 32;

	video_display_splash(gKernelArgs.frame_buffer.physical_buffer.start);

	return B_OK;

noFrameBuffer:
	gKernelArgs.frame_buffer.enabled = false;
	return B_ERROR;
}


extern "C" void
platform_switch_to_logo(void)
{
	if (gKernelArgs.frame_buffer.enabled) {
		sGraphicsOutput->SetMode(sGraphicsOutput, sScreenMode);
		video_display_splash(gKernelArgs.frame_buffer.physical_buffer.start);
	}
}


extern "C" void
platform_blit4(addr_t frameBuffer, const uint8 *data,
	uint16 width, uint16 height, uint16 imageWidth,
	uint16 left, uint16 top)
{
	return;
}


extern "C" void
platform_set_palette(const uint8 *palette)
{
	return;
}
