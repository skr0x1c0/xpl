//
//  kmem_tester.c
//  kmem_msdosfs
//
//  Created by admin on 11/29/21.
//

#include <time.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <stdlib.h>
#include <gym_client.h>

#include <xpl/memory/kmem.h>
#include <xpl/util/assert.h>

#include "memory/tester.h"


void xpl_kmem_random_fill_buffer(char* buffer, size_t buffer_len) {
    for (int i=0; i<buffer_len; i++) {
        buffer[i] = random() % UINT_MAX;
    }
}


void xpl_kmem_tester_read(size_t size, uint64_t* elapsed) {
    char write_buffer[size];
    xpl_kmem_random_fill_buffer(write_buffer, sizeof(write_buffer));
    
    slot_id_t slot;
    int error = gym_alloc_mem(sizeof(write_buffer), &slot);
    xpl_assert_err(error);
    error = gym_alloc_write(slot, write_buffer, sizeof(write_buffer));
    xpl_assert_err(error);
    uintptr_t slot_addr;
    error = gym_read_slot_address(slot, &slot_addr);
    xpl_assert_err(error);
    
    char read_buffer[size];
    uint64_t start = clock_gettime_nsec_np(CLOCK_MONOTONIC);
    xpl_kmem_read(read_buffer, slot_addr, 0, sizeof(read_buffer));
    *elapsed += (clock_gettime_nsec_np(CLOCK_MONOTONIC) - start);
    
    xpl_assert(memcmp(read_buffer, write_buffer, sizeof(write_buffer)) == 0);
    gym_alloc_free(slot);
    
    printf("[OK] test read of size %lu\n", size);
}

void xpl_kmem_tester_write(size_t size, uint64_t* elapsed) {
    slot_id_t slot;
    int error = gym_alloc_mem(size, &slot);
    xpl_assert_err(error);
    uintptr_t slot_addr;
    error = gym_read_slot_address(slot, &slot_addr);
    xpl_assert_err(error);
    
    char write_buffer[size];
    xpl_kmem_random_fill_buffer(write_buffer, sizeof(write_buffer));
    
    uint64_t start = clock_gettime_nsec_np(CLOCK_MONOTONIC);
    xpl_kmem_write(slot_addr, 0, write_buffer, sizeof(write_buffer));
    *elapsed += (clock_gettime_nsec_np(CLOCK_MONOTONIC) - start);
    
    char read_buffer[size];
    error = gym_alloc_read(slot, read_buffer, sizeof(read_buffer), NULL);
    xpl_assert_err(error);
    
    xpl_assert(memcmp(read_buffer, write_buffer, sizeof(write_buffer)) == 0);
    gym_alloc_free(slot);
    
    printf("[OK] test write of size %lu\n", size);
}

void xpl_kmem_tester_run(int count, size_t min_size, size_t max_size, double* read_bps, double* write_bps) {
    uint64_t total_read = 0;
    uint64_t total_wrote = 0;
    
    uint64_t total_read_time = 0;
    uint64_t total_write_time = 0;
    
    for (int i=0; i<count; i++) {
        size_t size = min_size + (random() % (max_size - min_size));
        size = size == 0 ? 1 : size;
        xpl_assert(size <= max_size);
        if (random() % 2 == 0) {
            xpl_kmem_tester_write(size, &total_write_time);
            total_wrote += size;
        } else {
            xpl_kmem_tester_read(size, &total_read_time);
            total_read += size;
        }
    }
    
    *read_bps = (double)total_read / ((double)total_read_time / 1e9);
    *write_bps = (double)total_wrote / ((double)total_write_time / 1e9);
}
