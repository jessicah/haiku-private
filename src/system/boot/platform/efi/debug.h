/*
 *
 *
 */
#ifndef DEBUG_H
#define DEBUG_H


#include <stdarg.h>

#include <SupportDefs.h>


#ifdef __cplusplus
extern "C" {
#endif

void debug_init_post_mmu(void);
void debug_cleanup(void);

#ifdef __cplusplus
}
#endif


#endif	// DEBUG_H
