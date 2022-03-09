//
//  ipc.c
//  libxe
//
//  Created by sreejith on 3/8/22.
//

#include <macos_params.h>

#include "xnu/ipc.h"
#include "memory/kmem.h"
#include "util/assert.h"
#include "util/log.h"


#define IE_BITS_TYPE_MASK       0x001f0000
#define IE_BITS_GEN_MASK        0xff000000


void xe_xnu_ipc_iter_mach_ports(uintptr_t proc, _Bool(^callback)(uint32_t name, uintptr_t port)) {
    uintptr_t task = xe_kmem_read_ptr(proc, TYPE_PROC_MEM_TASK_OFFSET);
    uintptr_t itk_space = xe_kmem_read_ptr(task, TYPE_TASK_MEM_ITK_SPACE_OFFSET);
    uintptr_t is_table_head = xe_kmem_read_ptr(itk_space, TYPE_IPC_SPACE_MEM_IS_TABLE_OFFSET);
    uint32_t is_table_size = xe_kmem_read_uint32(is_table_head, TYPE_IPC_ENTRY_MEM_IE_SIZE_OFFSET);
    xe_assert_cond(is_table_size & IE_BITS_TYPE_MASK, ==, 0);
    
    for (int i=0; i<is_table_size; i++) {
        uintptr_t entry = is_table_head + i * TYPE_IPC_ENTRY_SIZE;
        uintptr_t ie_object = xe_kmem_read_ptr(entry, TYPE_IPC_ENTRY_MEM_IE_OBJECT_OFFSET);
        
        if (ie_object) {
            uint32_t ie_bits = xe_kmem_read_uint32(entry, TYPE_IPC_ENTRY_MEM_IE_BITS_OFFSET);
            uint32_t name = (i << 8) | (ie_bits >> 24);
            
            if (callback(name, ie_object)) {
                break;
            }
        }
    }
}


int xe_xnu_ipc_find_mach_port(uintptr_t proc, uint32_t mach_port_name, uintptr_t* out) {
    int* error = alloca(sizeof(int));
    *error = ENOENT;
    xe_xnu_ipc_iter_mach_ports(proc, ^_Bool(uint32_t name, uintptr_t port) {
        if (name == mach_port_name) {
            *out = port;
            *error = 0;
            return 1;
        }
        return 0;
    });
    return *error;
}


uintptr_t xe_xnu_ipc_port_get_receiver_task(uintptr_t port) {
    uintptr_t ip_receiver = xe_kmem_read_ptr(port, TYPE_IPC_PORT_MEM_IP_RECEIVER_OFFSET);
    if (ip_receiver) {
        return xe_kmem_read_ptr(ip_receiver, TYPE_IPC_SPACE_MEM_IS_TASK_OFFSET);
    } else {
        return 0;
    }
}
