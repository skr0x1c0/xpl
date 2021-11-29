//
//  main.c
//  locker
//
//  Created by admin on 11/25/21.
//

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/sysctl.h>
#include <sys/errno.h>
#include <gym_client.h>

#include "kmem.h"
#include "slider.h"
#include "platform_types.h"
#include "platform_variables.h"


int main(int argc, const char * argv[]) {
    if (argc != 2) {
        printf("[ERROR] invalid arguments\n");
        abort();
    }
    
    char* endp;
    long duration = strtol(argv[1], &endp, 0);
    if (*endp != '\0' || duration > INT_MAX) {
        printf("[ERROR] invalid lock duration %s\n", argv[1]);
        abort();
    }
    
    xe_kmem_init();
    xe_slider_init();
    
    uintptr_t kheap_default = xe_slider_slide(VAR_KHEAP_DEFAULT);
    uintptr_t kh_large_map = xe_kmem_read_uint64(KMEM_OFFSET(kheap_default, TYPE_KALLOC_HEAP_MEM_KH_LARGE_MAP_OFFSET));
    uintptr_t lck_rw = KMEM_OFFSET(kh_large_map, TYPE_VM_MAP_MEM_LCK_RW_OFFSET);
    
    printf("[INFO] locking vm_map_t %p\n", (void*)kh_large_map);
    getpass("press enter to continue...\n");
    struct gym_lck_rw_hold_release_cmd cmd;
    cmd.lck_rw = lck_rw;
    cmd.seconds = (int)duration;
    cmd.exclusive = 0;
    
    if (sysctlbyname("kern.gym_lck_rw_hold_release", NULL, NULL, &cmd, sizeof(cmd))) {
        printf("[ERROR] gym lock failed, err: %d\n", errno);
        abort();
    }
    printf("[INFO] lock released\n");
    return 0;
}
