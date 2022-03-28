//
//  util_log.h
//  libxe
//
//  Created by admin on 12/28/21.
//

#ifndef xe_util_log_h
#define xe_util_log_h

#include "./binary.h"

#if 0
#define xe_log_verbose(...) printf("[V] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n")
#else
#define xe_log_verbose(...)
#endif

#define xe_log_debug(...) printf("[D] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n")
#define xe_log_info(...) printf("[I] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n")
#define xe_log_warn(...) fprintf(stderr, "[W] %s: ", __FUNCTION__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n")
#define xe_log_error(...) fprintf(stderr, "[E] %s: ", __FUNCTION__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");
#define xe_log_debug_hexdump(blob, size, ...) printf("[D] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n"); xe_util_binary_hex_dump((char*)blob, size)

void xe_log_print_backtrace(void);

#endif /* xe_util_log_h */
