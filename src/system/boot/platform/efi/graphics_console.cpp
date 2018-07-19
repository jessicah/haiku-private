/*
 * Copyright 2018 Haiku, Inc. All rights reserved.
 * Copyright 2018 Jessica Hamilton, jessica.l.hamilton@gmail.com. All rights
 * reserved.
 * Distributed under the terms of the MIT License.
 */


#define USE_GRAPHICS_CONSOLE
#ifdef USE_GRAPHICS_CONSOLE

#include "console.h"

#include <string.h>

#include <SupportDefs.h>

#include <boot/kernel_args.h>
#include <boot/stage2.h>
#include <boot/platform.h>
#include <boot/unifont.h>
#include <boot/platform/generic/video.h>
#include <util/kernel_cpp.h>

#include "efi_platform.h"

#define PIXEL_WIDTH 8
#define PIXEL_HEIGHT 18


extern "C" uint8*
video_load_font();


class GraphicsConsole : public ConsoleNode {
    public:
        GraphicsConsole();

        virtual ssize_t ReadAt(void *cookie, off_t pos, void *buffer,
            size_t bufferSize);
        virtual ssize_t WriteAt(void *cookie, off_t pos, const void *buffer,
            size_t bufferSize);

        void SetColor(int32 foreground, int32 background) { };
        void SetCursor(int32 x, int32 y) {
            fLeft = x * PIXEL_WIDTH; fTop = y * PIXEL_HEIGHT;
        }
        void ShowCursor() { };
        void HideCursor() { };

        void LoadFontData() {
		if (fFontBitmap == NULL)
            fFontBitmap = video_load_font();
        }
    
    private:
        uint8*  fFontBitmap;
        uint16  fLeft;
        uint16  fTop;
};


static GraphicsConsole sInput, sOutput;
FILE *stdin, *stdout, *stderr;


//  #pragma mark -


GraphicsConsole::GraphicsConsole()
    : ConsoleNode(),
    fFontBitmap(NULL),
    fLeft(0),
    fTop(0)
{
}


ssize_t
GraphicsConsole::ReadAt(void *cookie, off_t pos, void *buffer, size_t bufferSize)
{
    return B_ERROR;
}


ssize_t
GraphicsConsole::WriteAt(void *cookie, off_t /*pos*/, const void *buffer,
    size_t bufferSize)
{
    const char *string = (const char *)buffer;
//	dprintf("GraphicsConsole::WriteAt()\n");
//	LoadFontData();
//	platform_switch_to_logo();
if (!gKernelArgs.frame_buffer.enabled) {
	dprintf("%s", string);
	return bufferSize;
} else {
	if (fFontBitmap == NULL)
		platform_switch_to_logo();
	LoadFontData();
}

    for (size_t i = 0; i < bufferSize; i++) {
        switch (string[i]) {
            case '\n': {
                // update position, y += line height, x = 0
                fLeft = 0;
                fTop += PIXEL_HEIGHT;
                continue;
            }
            case ' ': {
                // update position, x += character width
                fLeft += PIXEL_WIDTH;
                continue;
            }
            default: {
                // display character, x += character width
                int charIndex = string[i] - '!';
		dprintf("output %c, index %d, left %d, top %d\n", string[i], charIndex, fLeft, fTop);
                uint8 *bitmapStart = fFontBitmap + (charIndex * PIXEL_WIDTH * 3);
                video_blit_image(gKernelArgs.frame_buffer.physical_buffer.start,
                    bitmapStart, PIXEL_WIDTH, PIXEL_HEIGHT, kUnifontImageWidth,
                    fLeft, fTop);
                fLeft += PIXEL_WIDTH;
//dprintf("framebuffer at %p, bitmap character at %p\n", (void*)gKernelArgs.frame_buffer.physical_buffer.start,
//	bitmapStart);
            }
        }
    }

//dprintf("printed %s to screen\n", string);
//	panic("stuff aint working\n");
    return bufferSize;
}


//	#pragma mark -


void
console_clear_screen(void)
{
	// TODO
}


int32
console_width(void)
{
	return gKernelArgs.frame_buffer.width / PIXEL_WIDTH;
}


int32
console_height(void)
{
	return gKernelArgs.frame_buffer.height / PIXEL_HEIGHT;
}


void
console_set_cursor(int32 x, int32 y)
{
	sOutput.SetCursor(x, y);
}


void
console_show_cursor(void)
{
	// TODO
}


void
console_hide_cursor(void)
{
	// TODO
}


void
console_set_color(int32 foreground, int32 background)
{
	sOutput.SetColor(foreground, background);
}


int
console_wait_for_key(void)
{
	UINTN index;
	EFI_STATUS status;
	EFI_INPUT_KEY key;
	EFI_EVENT event = kSystemTable->ConIn->WaitForKey;

	do {
		kBootServices->WaitForEvent(1, &event, &index);
		status = kSystemTable->ConIn->ReadKeyStroke(kSystemTable->ConIn, &key);
	} while (status == EFI_NOT_READY);

	if (key.UnicodeChar > 0)
		return (int) key.UnicodeChar;

	switch (key.ScanCode) {
		case SCAN_UP:
			return TEXT_CONSOLE_KEY_UP;
		case SCAN_DOWN:
			return TEXT_CONSOLE_KEY_DOWN;
		case SCAN_LEFT:
			return TEXT_CONSOLE_KEY_LEFT;
		case SCAN_RIGHT:
			return TEXT_CONSOLE_KEY_RIGHT;
		case SCAN_PAGE_UP:
			return TEXT_CONSOLE_KEY_PAGE_UP;
		case SCAN_PAGE_DOWN:
			return TEXT_CONSOLE_KEY_PAGE_DOWN;
		case SCAN_HOME:
			return TEXT_CONSOLE_KEY_HOME;
		case SCAN_END:
			return TEXT_CONSOLE_KEY_END;
	}
	return 0;
}


status_t
console_init(void)
{
	console_hide_cursor();
	console_clear_screen();

	// enable stdio functionality
	stdin = (FILE *)&sInput;
	stdout = stderr = (FILE *)&sOutput;

	return B_OK;
}


uint32
console_check_boot_keys(void)
{
	EFI_STATUS status;
	EFI_INPUT_KEY key;

	// give the user a chance to press a key
	kBootServices->Stall(500000);

	status = kSystemTable->ConIn->ReadKeyStroke(kSystemTable->ConIn, &key);

	if (status != EFI_SUCCESS)
		return 0;

	if (key.UnicodeChar == 0 && key.ScanCode == SCAN_ESC)
		return BOOT_OPTION_DEBUG_OUTPUT;
	if (key.UnicodeChar == ' ')
		return BOOT_OPTION_MENU;

	return 0;
}


extern "C" void
platform_switch_to_text_mode(void)
{
	// DO NOTHING
//	platform_switch_to_logo();
}


#endif // USE_GRAPHICS_CONSOLE
