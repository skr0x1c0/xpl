//
//  thread.h
//  libxe
//
//  Created by sreejith on 2/25/22.
//

#ifndef xpl_thread_h
#define xpl_thread_h

#include <stdio.h>

uintptr_t xpl_thread_current_thread(void);
int xpl_thread_scan_stack(uintptr_t thread, uintptr_t value, uintptr_t mask, int scan_count, uintptr_t* found_address);

#endif /* xpl_thread_h */
