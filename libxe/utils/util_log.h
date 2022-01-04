//
//  util_log.h
//  libxe
//
//  Created by admin on 12/28/21.
//

#ifndef util_log_h
#define util_log_h

#define XE_LOG_DEBUG(...) printf("[DEBUG] "); printf(__VA_ARGS__); printf("\n");
#define XE_LOG_INFO(...) printf("[INFO] "); printf(__VA_ARGS__); printf("\n");
#define XE_LOG_WARN(...) printf("[WARN] "); printf(__VA_ARGS__); printf("\n");

#endif /* util_log_h */
