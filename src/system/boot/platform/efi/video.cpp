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


extern "C" void
platform_switch_to_logo(void)
{
	return;
}


extern "C" status_t
platform_init_video()
{
	EFI_GRAPHICS_OUTPUT_PROTOCOL *graphicsOutput;
	EFI_STATUS status = kBootServices->LocateProtocol(&sGraphicsOutputGuid,
		NULL, (void **)&graphicsOutput);
	if (graphicsOutput == NULL) {
		gKernelArgs.frame_buffer.enabled = false;
		return B_ERROR;
	}

	UINTN bestMode = 0;
	UINTN bestArea = 0;

	for (UINTN i = 0; i < graphicsOutput->Mode->MaxMode; ++i) {
		EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
		UINTN size;
		graphicsOutput->QueryMode(graphicsOutput, i, &size, &info);
		if (info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor
			&& info->HorizontalResolution * info->VerticalResolution >= bestArea) {
			bestMode = i;
			bestArea = info->HorizontalResolution * info->VerticalResolution;
		}
	}

	graphicsOutput->SetMode(graphicsOutput, bestMode);

	gKernelArgs.frame_buffer.enabled = true;
	gKernelArgs.frame_buffer.physical_buffer.start =
		graphicsOutput->Mode->FrameBufferBase;
	gKernelArgs.frame_buffer.physical_buffer.size =
		graphicsOutput->Mode->FrameBufferSize;
	gKernelArgs.frame_buffer.width =
		graphicsOutput->Mode->Info->HorizontalResolution;
	gKernelArgs.frame_buffer.height =
		graphicsOutput->Mode->Info->VerticalResolution;
	gKernelArgs.frame_buffer.bytes_per_row =
		graphicsOutput->Mode->Info->PixelsPerScanLine * 4;
	gKernelArgs.frame_buffer.depth = 32;

	video_display_splash(gKernelArgs.frame_buffer.physical_buffer.start);
	return B_OK;
}
