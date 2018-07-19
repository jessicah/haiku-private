/*
** Copyright 2004, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
** Distributed under the terms of the Haiku License.
*/
#ifndef CONSOLE_H
#define CONSOLE_H


#include <boot/platform/generic/text_console.h>


status_t console_init(void);
uint32 console_check_boot_keys(void);


class ConsoleBase : public ConsoleNode {
    public:
        ConsoleBase() { };

        virtual void SetColor(int32 foreground, int32 background) = 0;
        virtual void SetCursor(int32 x, int32 y) = 0;
        virtual void ShowCursor() = 0;
        virtual void HideCursor() = 0;
};


#endif	/* CONSOLE_H */
