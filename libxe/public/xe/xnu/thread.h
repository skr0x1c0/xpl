//
//  thread.h
//  libxe
//
//  Created by sreejith on 2/25/22.
//

#ifndef thread_h
#define thread_h

#include <stdio.h>

uintptr_t xe_xnu_thread_current_thread(void);
int xe_xnu_thread_scan_stack(uintptr_t thread, uintptr_t value, uintptr_t mask, int scan_count, uintptr_t* found_address);

#endif /* thread_h */
