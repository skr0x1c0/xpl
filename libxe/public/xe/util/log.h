//
//  util_log.h
//  libxe
//
//  Created by admin on 12/28/21.
//

#ifndef xe_util_log_h
#define xe_util_log_h

#include "binary.h"

#define xe_log_debug(...) printf("[D] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n")
#define xe_log_info(...) printf("[I] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n")
#define xe_log_warn(...) printf("[W] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n")
#define xe_log_error(...) printf("[E] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n")
#define xe_log_debug_hexdump(blob, size, ...) printf("[D] %s: ", __FUNCTION__); printf(__VA_ARGS__); printf("\n"); xe_util_binary_hex_dump((char*)blob, size)

#endif /* xe_util_log_h */
