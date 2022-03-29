//
//  slider_kas.c
//  libxe_dev
//
//  Created by admin on 1/11/22.
//

#include <assert.h>

#include "gym_client.h"


uintptr_t xpl_slider_kas_get_mh_execute_header(void) {
    uintptr_t base;
    int error = gym_mach_header(&base);
    assert(error == 0);
    return base;
}
