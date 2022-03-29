//
//  log.c
//  libxe
//
//  Created by sreejith on 3/18/22.
//

#include <libunwind.h>

#include "util/log.h"


void xpl_log_print_backtrace(void) {
    unw_cursor_t cursor;
    unw_context_t context;
    
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);
    
    while (unw_step(&cursor)) {
        unw_word_t ip, sp, off;
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        unw_get_reg(&cursor, UNW_REG_SP, &sp);
        
        char symbol[256] = { '\0' };
        unw_get_proc_name(&cursor, symbol, sizeof(symbol), &off);
        
        xpl_log_debug("%s: %p + %p sp=%p", symbol, (void*)ip, (void*)off, (void*)sp);
    }
}
