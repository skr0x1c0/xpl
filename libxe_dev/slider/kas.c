//
//  slider_kas.c
//  libxe_dev
//
//  Created by admin on 1/11/22.
//

#include <sys/errno.h>

#include "slider/kas.h"


/* The slide of the main kernel compared to its static link address */
#define KAS_INFO_KERNEL_TEXT_SLIDE_SELECTOR         (0)     /* returns uint64_t    */
#define KAS_INFO_KERNEL_SEGMENT_VMADDR_SELECTOR     (1)
#define KAS_INFO_MAX_SELECTOR                       (2)

extern int kas_info(int selector, void *value, size_t *size);


uintptr_t xe_slider_kas_get_mh_execute_header(void) {
    size_t num_segments = 0;
    if (kas_info(KAS_INFO_KERNEL_SEGMENT_VMADDR_SELECTOR, NULL, &num_segments)) {
        return errno;
    }
    
    uintptr_t kern_segment_bases[num_segments];
    if (kas_info(KAS_INFO_KERNEL_SEGMENT_VMADDR_SELECTOR, kern_segment_bases, &num_segments)) {
        return errno;
    }
    
    return kern_segment_bases[0];
}
