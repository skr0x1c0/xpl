#include <stdatomic.h>

#include <IOKit/IOKitLib.h>
#include <IOSurface/IOSurface.h>

#include "allocator_iosurface.h"


struct iokit_iosurface_allocator {
    _Atomic size_t alloc_idx;
    IOSurfaceRef surface;
};


int iokit_iosurface_allocator_create(iokit_iosurface_allocator_t* allocator_out) {
    CFMutableDictionaryRef props = CFDictionaryCreateMutable(CFAllocatorGetDefault(), 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    int alloc_size_raw_value = 8;
    CFNumberRef alloc_size_cfnum = CFNumberCreate(NULL, kCFNumberSInt32Type, &alloc_size_raw_value);
    CFDictionarySetValue(props, CFSTR("IOSurfaceAllocSize"), alloc_size_cfnum);
    CFDictionarySetValue(props, CFSTR("IOSurfaceIsGlobal"), kCFBooleanTrue);

    IOSurfaceRef surface = IOSurfaceCreate(props);
    if (!surface) {
        return EFAULT;
    }

    iokit_iosurface_allocator_t allocator = (iokit_iosurface_allocator_t)malloc(sizeof(struct iokit_iosurface_allocator));
    allocator->surface = surface;
    atomic_init(&allocator->alloc_idx, 0);
    *allocator_out = allocator;

    CFRelease(props);
    return 0;
}

int iokit_iosurface_allocator_allocate(iokit_iosurface_allocator_t allocator, int size, int count, size_t* alloc_idx_out) {
    if (size % 8 != 0) {
        return EINVAL;
    }

    int array_size = size / 8;
    CFNullRef refs[array_size];
    for (int i=0; i<size / 8; i++) {
        refs[i] = kCFNull;
    }

    CFTypeRef data;
    if (count > 1) {
        CFArrayRef elements[count];
        for (int i=0; i<count; i++) {
            elements[i] = CFArrayCreate(kCFAllocatorDefault, (const void**)&refs, array_size, &kCFTypeArrayCallBacks);
        }
        data = CFArrayCreate(kCFAllocatorDefault, (const void**)elements, count, &kCFTypeArrayCallBacks);
        for (int i=0; i<count; i++) {
            CFRelease(elements[i]);
        }
    } else {
        data = CFArrayCreate(kCFAllocatorDefault, (const void**)&refs, array_size, &kCFTypeArrayCallBacks);
    }
    if (!data) {
        return ENOMEM;
    }

    size_t alloc_idx = atomic_fetch_add(&allocator->alloc_idx, 1);
    char key_raw[NAME_MAX];
    snprintf(key_raw, sizeof(key_raw), "iosurface_alloc_%lu", alloc_idx);

    CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault, key_raw, kCFStringEncodingUTF8);
    IOSurfaceSetValue(allocator->surface, key, data);
    if (alloc_idx_out != NULL) {
        *alloc_idx_out = alloc_idx;
    }

    CFRelease(data);
    CFRelease(key);
    return 0;
}

int iokit_iosurface_allocator_release(iokit_iosurface_allocator_t allocator, size_t alloc_idx) {
    char key_raw[NAME_MAX];
    snprintf(key_raw, sizeof(key_raw), "iosurface_alloc_%lu", alloc_idx);
    CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault, key_raw, kCFStringEncodingUTF8);
    IOSurfaceRemoveValue(allocator->surface, key);
    CFRelease(key);
    return 0;
}

int iokit_iosurface_allocator_destroy(iokit_iosurface_allocator_t* allocator_p) {
    iokit_iosurface_allocator_t allocator = *allocator_p;
    IOSurfaceRemoveAllValues(allocator->surface);
    IOSurfaceDecrementUseCount(allocator->surface);
    free(allocator);
    *allocator_p = NULL;
    return 0;
}
