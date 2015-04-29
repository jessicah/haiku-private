/*
 * Copyright 2014, Jessica Hamilton, jessica.l.hamilton@gmail.com.
 * Distributed under the terms of the MIT License.
 */


#include "efi_platform.h"

#include <boot/stage2.h>
#include <boot/platform.h>
#include <boot/kernel_args.h>
#include <boot/platform/generic/video.h>


static EFI_GUID sGraphicsOutputGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_GRAPHICS_OUTPUT_PROTOCOL *sGraphicsOutput;
static UINTN sBestMode;

extern "C" void
platform_switch_to_logo(void)
{
	if (sGraphicsOutput == NULL)
		return;

	sGraphicsOutput->SetMode(sGraphicsOutput, sBestMode);
	video_display_splash(gKernelArgs.frame_buffer.physical_buffer.start);
}


extern "C" status_t
platform_init_video()
{
	EFI_STATUS status = kBootServices->LocateProtocol(&sGraphicsOutputGuid,
		NULL, (void **)&sGraphicsOutput);
	if (status != EFI_SUCCESS || sGraphicsOutput == NULL) {
		gKernelArgs.frame_buffer.enabled = false;
		return B_ERROR;
	}

	UINTN bestArea = 0;

	for (UINTN mode = 0; mode < sGraphicsOutput->Mode->MaxMode; ++mode) {
		EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
		UINTN size;
		sGraphicsOutput->QueryMode(sGraphicsOutput, mode, &size, &info);
		if (info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor
			&& info->HorizontalResolution * info->VerticalResolution >= bestArea) {
			sBestMode = mode;
			bestArea = info->HorizontalResolution * info->VerticalResolution;
		}
	}

	sGraphicsOutput->SetMode(sGraphicsOutput, sBestMode);

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
}
