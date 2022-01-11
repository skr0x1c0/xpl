//
//  util_log.h
//  libxe
//
//  Created by admin on 12/28/21.
//

#ifndef util_log_h
#define util_log_h

#include "binary.h"

#define xe_log_debug(...) printf("[DEBUG] "); printf(__VA_ARGS__); printf("\n")
#define xe_log_info(...) printf("[INFO] "); printf(__VA_ARGS__); printf("\n")
#define xe_log_warn(...) printf("[WARN] "); printf(__VA_ARGS__); printf("\n")
#define xe_log_error(...) printf("[ERROR] "); printf(__VA_ARGS__); printf("\n")
#define xe_log_debug_hexdump(blob, size, ...) printf("[DEBUG] "); printf(__VA_ARGS__); printf("\n"); xe_util_binary_hex_dump((char*)blob, size)

#endif /* util_log_h */
