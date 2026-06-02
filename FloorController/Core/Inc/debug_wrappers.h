/**
******************************************************************************** 
* @file     : debug_wrappers.h
* @brief    : Defines debug redirects and functions which do not run in release
* By        : Nigel Sinclair
******************************************************************************** 
*/
#ifndef __DEBUG_WRAPPERS_H
#define __DEBUG_WRAPPERS_H

#if !defined(NDEBUG)
#include <stdio.h>
#include "main.h"

#define dbglog(str, ...) printf(str __VA_OPT__(,) __VA_ARGS__)

/**
 * @brief:  Debug panic handler wrapper for printing location of panic
 * @param:  File that the error occurred in (__FILE__)
 * @param:  Line number that the error occured on (__LINE__)
 * @param:  Message to print
 */
static inline void __dbg_panic(const char* file, int line, const char* msg) {
    dbglog("ERROR: in file %s on line %d - '%s'\n", file, line, msg);
    panic();
}

/**
 * @brief:      Wrapper macro for replacing panic calls with debug versions
 * @param:      Message to print
 */
#define panic(msg) __dbg_panic(__FILE__, __LINE__, msg)

#else

#define dbglog(unused, ...)
#define panic(unused) panic()

#endif

#endif
