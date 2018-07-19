/*
 * Copyright 2004-2009, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2008, Stephan Aßmus <superstippi@gmx.de>
 * Copyright 2008, Philippe Saint-Pierre <stpere@gmail.com>
 * Distributed under the terms of the MIT License.
 */


#include <arch/cpu.h>
#include <boot/stage2.h>
#include <boot/platform.h>
#include <boot/platform/generic/video.h>
#include <boot/kernel_args.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define TRACE_VIDEO
#ifdef TRACE_VIDEO
#	define TRACE(x) dprintf x
#else
#	define TRACE(x) ;
#endif


static void
blit32(addr_t frameBuffer, const uint8 *data, uint16 width,
	uint16 height, uint16 imageWidth, uint16 left, uint16 top)
{
	uint32 *start = (uint32 *)(frameBuffer
		+ gKernelArgs.frame_buffer.bytes_per_row * top + 4 * left);

	for (int32 y = 0; y < height; y++) {
		const uint8* src = data;
		uint32* dst = start;
		for (int32 x = 0; x < width; x++) {
			dst[0] = (src[2] << 16) | (src[1] << 8) | (src[0]);
			dst++;
			src += 3;
		}

		data += imageWidth * 3;
		start = (uint32 *)((addr_t)start
			+ gKernelArgs.frame_buffer.bytes_per_row);
	}
}


static void
blit24(addr_t frameBuffer, const uint8 *data, uint16 width,
	uint16 height, uint16 imageWidth, uint16 left, uint16 top)
{
	uint8 *start = (uint8 *)frameBuffer
		+ gKernelArgs.frame_buffer.bytes_per_row * top + 3 * left;

	for (int32 y = 0; y < height; y++) {
		const uint8* src = data;
		uint8* dst = start;
		for (int32 x = 0; x < width; x++) {
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst += 3;
			src += 3;
		}

		data += imageWidth * 3;
		start = start + gKernelArgs.frame_buffer.bytes_per_row;
	}
}


static void
blit16(addr_t frameBuffer, const uint8 *data, uint16 width,
	uint16 height, uint16 imageWidth, uint16 left, uint16 top)
{
	uint16 *start = (uint16 *)(frameBuffer
		+ gKernelArgs.frame_buffer.bytes_per_row * top + 2 * left);

	for (int32 y = 0; y < height; y++) {
		const uint8* src = data;
		uint16* dst = start;
		for (int32 x = 0; x < width; x++) {
			dst[0] = ((src[2] >> 3) << 11)
				| ((src[1] >> 2) << 5)
				| ((src[0] >> 3));
			dst++;
			src += 3;
		}

		data += imageWidth * 3;
		start = (uint16 *)((addr_t)start
			+ gKernelArgs.frame_buffer.bytes_per_row);
	}
}


static void
blit15(addr_t frameBuffer, const uint8 *data, uint16 width,
	uint16 height, uint16 imageWidth, uint16 left, uint16 top)
{
	uint16 *start = (uint16 *)(frameBuffer
		+ gKernelArgs.frame_buffer.bytes_per_row * top + 2 * left);

	for (int32 y = 0; y < height; y++) {
		const uint8* src = data;
		uint16* dst = start;
		for (int32 x = 0; x < width; x++) {
			dst[0] = ((src[2] >> 3) << 10)
				| ((src[1] >> 3) << 5)
				| ((src[0] >> 3));
			dst++;
			src += 3;
		}

		data += imageWidth * 3;
		start = (uint16 *)((addr_t)start
			+ gKernelArgs.frame_buffer.bytes_per_row);
	}
}


static void
blit8(addr_t frameBuffer, const uint8 *data, uint16 width,
	uint16 height, uint16 imageWidth, uint16 left, uint16 top)
{
	if (!data)
		return;

	addr_t start = frameBuffer + gKernelArgs.frame_buffer.bytes_per_row * top
		+ left;

	for (int32 i = 0; i < height; i++) {
		memcpy((void *)(start + gKernelArgs.frame_buffer.bytes_per_row * i),
			&data[i * imageWidth], width);
	}
}


static void
blit4(addr_t frameBuffer, const uint8 *data, uint16 width,
	uint16 height, uint16 imageWidth, uint16 left, uint16 top)
{
	if (!data)
		return;
	// call back platform specific code since it's really platform-specific.
	platform_blit4(frameBuffer, data, width, height,
		imageWidth, left, top);
}


void
video_blit_image(addr_t frameBuffer, const uint8 *data,
	uint16 width, uint16 height, uint16 imageWidth, uint16 left, uint16 top)
{
	switch (gKernelArgs.frame_buffer.depth) {
		case 4:
			return blit4(frameBuffer, data,
				width, height, imageWidth, left, top);
		case 8:
			return blit8(frameBuffer, data,
				width, height, imageWidth, left, top);
		case 15:
			return blit15(frameBuffer, data,
				width, height, imageWidth, left, top);
		case 16:
			return blit16(frameBuffer, data,
				width, height, imageWidth, left, top);
		case 24:
			return blit24(frameBuffer, data,
				width, height, imageWidth, left, top);
		case 32:
			return blit32(frameBuffer, data,
				width, height, imageWidth, left, top);
	}
}


static void
blit_mask32(addr_t frameBuffer, const uint8 *data, int32 fore, int32 back,
	uint16 width, uint16 height, uint16 imageWidth, uint16 left, uint16 top)
{
	uint32 *start = (uint32 *)(frameBuffer
		+ gKernelArgs.frame_buffer.bytes_per_row * top + 4 * left);

	for (int32 y = 0; y < height; y++) {
		const uint8* src = data;
		uint32* dst = start;
		for (int32 x = 0; x < width; x++) {
			if (src[0] == 255 && src[1] == 255 && src[2] == 255) {
				dst[0] = fore;
			} else {
				dst[0] = back;
			}
			dst++;
			src += 3;
		}

		data += imageWidth * 3;
		start = (uint32 *)((addr_t)start
			+ gKernelArgs.frame_buffer.bytes_per_row);
	}
}


static void
blit_mask24(addr_t frameBuffer, const uint8 *data, int32 fore, int32 back,
	uint16 width, uint16 height, uint16 imageWidth, uint16 left, uint16 top)
{
	uint8 *start = (uint8 *)frameBuffer
		+ gKernelArgs.frame_buffer.bytes_per_row * top + 3 * left;

	for (int32 y = 0; y < height; y++) {
		const uint8* src = data;
		uint8* dst = start;
		for (int32 x = 0; x < width; x++) {
			if (src[0] == 0 && src[1] == 0 && src[2] == 0) {
				dst[0] = (back >> 16) & 0xFF;
				dst[1] = (back >> 8) & 0xFF;
				dst[2] = back & 0xFF;
			} else {
				float r = (float)src[0] / 255.0;
				float g = (float)src[1] / 255.0;
				float b = (float)src[2] / 255.0;
				r *= (fore >> 16) & 0xFF;
				g *= (fore >> 8) & 0xFF;
				b *= fore & 0xFF;
				dst[0] = (uint8)r;
				dst[1] = (uint8)g;
				dst[2] = (uint8)b;
			}
			dst += 3;
			src += 3;
		}

		data += imageWidth * 3;
		start = start + gKernelArgs.frame_buffer.bytes_per_row;
	}
}


void
video_blit_image_mask(addr_t frameBuffer, const uint8 *data, int32 fore,
	int32 back, uint16 width, uint16 height, uint16 imageWidth,
	uint16 left, uint16 top)
{
	switch (gKernelArgs.frame_buffer.depth) {
		case 24:
			return blit_mask24(frameBuffer, data, fore, back,
				width, height, imageWidth, left, top);
		case 32:
			return blit_mask32(frameBuffer, data, fore, back,
				width, height, imageWidth, left, top);
		default:
			return;
	}
}

