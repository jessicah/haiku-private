/*
 * Copyright 2009, Haiku Inc.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef GENERIC_VIDEO_H
#define GENERIC_VIDEO_H


#include <SupportDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/* load font.h for use by graphical console */
uint8 *video_load_font();

/* blit helpers */

/* platform code is responsible for setting the palette correctly */
void video_blit_image(addr_t frameBuffer, const uint8 *data,
	uint16 width, uint16 height, uint16 imageWidth,
	uint16 left, uint16 top);

/* variation that blends foreground and background colours,
   primarily used for anti-aliased font data */
void video_blit_image_mask(addr_t frameBuffer, const uint8 *data,
	int32 fore, int32 back, uint16 width, uint16 height,
	uint16 imageWidth, uint16 left, uint16 top);

/* platform code must implement 4bit on its own */
void platform_blit4(addr_t frameBuffer, const uint8 *data,
	uint16 width, uint16 height, uint16 imageWidth,
	uint16 left, uint16 top);
void platform_set_palette(const uint8 *palette);

/* Run Length Encoding splash decompression */

void uncompress_24bit_RLE(const uint8 compressed[], uint8 *uncompressed);
void uncompress_8bit_RLE(const uint8 compressed[], uint8 *uncompressed);

/* default splash display */
status_t video_display_splash(addr_t frameBuffer);

#ifdef __cplusplus
}
#endif

#endif	/* GENERIC_VIDEO_H */
