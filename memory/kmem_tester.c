//
//  kmem_tester.c
//  kmem_msdosfs
//
//  Created by admin on 11/29/21.
//

#include <string.h>
#include <limits.h>
#include <assert.h>
#include <stdlib.h>
#include <gym_client.h>

#include "kmem.h"
#include "kmem_tester.h"


void xe_kmem_random_fill_buffer(char* buffer, size_t buffer_len) {
    for (int i=0; i<buffer_len; i++) {
        buffer[i] = random() % UINT_MAX;
    }
}


void xe_kmem_tester_read(size_t size) {
    char write_buffer[size];
    xe_kmem_random_fill_buffer(write_buffer, sizeof(write_buffer));
    
    slot_id_t slot;
    int error = gym_alloc_mem(sizeof(write_buffer), &slot);
    assert(error == 0);
    error = gym_alloc_write(slot, write_buffer, sizeof(write_buffer));
    assert(error == 0);
    uintptr_t slot_addr;
    error = gym_read_slot_address(slot, &slot_addr);
    assert(error == 0);
    
    char read_buffer[size];
    xe_kmem_read(read_buffer, slot_addr, sizeof(read_buffer));
    assert(memcmp(read_buffer, write_buffer, sizeof(write_buffer)) == 0);
    gym_alloc_free(slot);
    
    printf("[OK] test read of size %lu\n", size);
}

void xe_kmem_tester_write(size_t size) {
    slot_id_t slot;
    int error = gym_alloc_mem(size, &slot);
    assert(error == 0);
    uintptr_t slot_addr;
    error = gym_read_slot_address(slot, &slot_addr);
    assert(error == 0);
    
    char write_buffer[size];
    xe_kmem_random_fill_buffer(write_buffer, sizeof(write_buffer));
    xe_kmem_write(slot_addr, write_buffer, sizeof(write_buffer));
    
    char read_buffer[size];
    error = gym_alloc_read(slot, read_buffer, sizeof(read_buffer), NULL);
    assert(error == 0);
    
    assert(memcmp(read_buffer, write_buffer, sizeof(write_buffer)) == 0);
    gym_alloc_free(slot);
    
    printf("[OK] test write of size %lu\n", size);
}

void xe_kmem_tester_run(int count, size_t max_size) {
    for (int i=0; i<count; i++) {
        size_t size = random() % max_size;
        size = size == 0 ? 1 : size;
        assert(size <= max_size);
        if (random() % 2 == 0) {
            xe_kmem_tester_write(size);
        } else {
            xe_kmem_tester_read(size);
        }
    }
}
