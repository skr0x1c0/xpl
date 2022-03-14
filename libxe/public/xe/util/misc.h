//
//  utils_misc.h
//  xe
//
//  Created by admin on 11/26/21.
//

#ifndef xe_util_misc_h
#define xe_util_misc_h

#include <time.h>
#include <sys/sysctl.h>

#include "./assert.h"

#define xe_array_size(arr) (sizeof(arr) / sizeof((arr)[0]))
#define xe_min(a, b) ((a) < (b) ? (a) : (b))
#define xe_max(a, b) ((a) > (b) ? (a) : (b))
#define xe_bitfield_mask(off, size) (((1ULL << (size)) - 1) << (off))


static inline int xe_sleep_ms(uint64_t ms) {
    struct timespec rqp;
    rqp.tv_sec = ms / 1000;
    rqp.tv_nsec = (ms - (rqp.tv_sec * 1000)) * 1000 * 1000;
    nanosleep(&rqp, NULL);
    return 0;
}


static inline size_t xe_cpu_count(void) {
    size_t count;
    size_t out_size = sizeof(count);
    int res = sysctlbyname("hw.ncpu", &count, &out_size, NULL, 0);
    xe_assert_errno(res);
    return count;
}


#endif /* xe_util_misc_h */
