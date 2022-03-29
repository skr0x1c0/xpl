//
//  util_log.h
//  libxe
//
//  Created by admin on 12/28/21.
//

#ifndef xpl_util_log_h
#define xpl_util_log_h

#include "./binary.h"

#if 0
#define xpl_log_verbose(...) printf("[V] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n")
#else
#define xpl_log_verbose(...)
#endif

#if DEBUG
#define xpl_log_debug(...) printf("[D] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n")
#else
#define xpl_log_debug(...) 
#endif

#define xpl_log_info(...) printf("[I] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n")
#define xpl_log_warn(...) fprintf(stderr, "[W] %s: ", __FUNCTION__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n")
#define xpl_log_error(...) fprintf(stderr, "[E] %s: ", __FUNCTION__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");
#define xpl_log_debug_hexdump(blob, size, ...) printf("[D] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n"); xpl_util_binary_hex_dump((char*)blob, size)

void xpl_log_print_backtrace(void);

#endif /* xpl_util_log_h */
