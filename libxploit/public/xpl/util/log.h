//
//  util_log.h
//  libxe
//
//  Created by admin on 12/28/21.
//

#ifndef xpl_log_h
#define xpl_log_h

#include "./binary.h"

#define XE_LOG_MODE_VERBOSE 10
#define XE_LOG_MODE_DEBUG 9
#define XE_LOG_MODE_INFO 8


#if !defined(XE_LOG_MODE)
#if DEBUG
#define XE_LOG_MODE XE_LOG_MODE_DEBUG
#else
#define XE_LOG_MODE XE_LOG_MODE_INFO
#endif /* DEBUG */
#endif /* XE_LOG_MODE */


#if XE_LOG_MODE >= XE_LOG_MODE_VERBOSE
#define xpl_log_verbose(...) printf("[V] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n")
#else
#define xpl_log_verbose(...)
#endif

#if XE_LOG_MODE >= XE_LOG_MODE_DEBUG
#define xpl_log_debug(...) printf("[D] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n")
#define xpl_log_debug_hexdump(blob, size, ...) printf("[D] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n"); xpl_binary_hex_dump((char*)blob, size)
#else
#define xpl_log_debug(...)
#define xpl_log_debug_hexdump(blob, size, ...)
#endif

#if XE_LOG_MODE >= XE_LOG_MODE_INFO
#define xpl_log_info(...) printf("[I] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n")
#else
#define xpl_log_info(...)
#endif

#define xpl_log_warn(...) fprintf(stderr, "[W] %s: ", __FUNCTION__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n")

#define xpl_log_error(...) fprintf(stderr, "[E] %s: ", __FUNCTION__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");

void xpl_log_print_backtrace(void);

#endif /* xpl_log_h */
