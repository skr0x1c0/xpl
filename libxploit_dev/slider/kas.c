
#include <assert.h>

#include "gym_client.h"


uintptr_t xpl_slider_kas_get_mh_execute_header(void) {
    uintptr_t base;
    int error = gym_mach_header(&base);
    assert(error == 0);
    return base;
}
