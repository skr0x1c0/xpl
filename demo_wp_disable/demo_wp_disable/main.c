//
//  main.c
//  demo_wp_disable
//
//  Created by admin on 2/18/22.
//


#include <stdio.h>
#include <limits.h>

#include <xe/xe.h>
#include <xe/memory/kmem.h>
#include <xe/memory/kmem_remote.h>
#include <xe/slider/kernel.h>
#include <xe/slider/kext.h>
#include <xe/iokit/io_registry_entry.h>
#include <xe/iokit/os_object.h>
#include <xe/iokit/os_array.h>
#include <xe/iokit/os_dictionary.h>
#include <xe/util/log.h>
#include <xe/util/assert.h>
#include <xe/util/misc.h>

#define VAR_IO_MEDIA_META_CLASS_DATA_OFFSET 0x310
#define VAR_IO_STORAGE_META_CLASS_DATA_OFFSET 0x388


int make_dev_mount_writable(const char* dev) {
    char cmd[PATH_MAX];
    snprintf(cmd, sizeof(cmd), "diskutil mount -mountOptions \"rw,update\" /dev/%s", dev);
    
    int result = system(cmd);
    if (result == 0) {
        return 0;
    }
    
    xe_log_warn("update mount with rw failed, trying remount");
    
    snprintf(cmd, sizeof(cmd), "diskutil unmount /dev/%s", dev);
    result = system(cmd);
    if (result != 0) {
        xe_log_error("diskutil unmount failed, exit code: %d", result);
        return EBADF;
    }
    
    snprintf(cmd, sizeof(cmd), "diskutil mount -mountOptions \"rw\" /dev/%s", dev);
    result = system(cmd);
    if (result != 0) {
        xe_log_error("diskutil mount failed, exit code: %d", result);
        return EBADF;
    }
    
    return 0;
}


void recursive_patch_storage(xe_slider_kext_t io_storage_slider, uintptr_t storage) {
    uintptr_t io_storage_meta_class = xe_slider_kext_slide(io_storage_slider, XE_KEXT_SEGMENT_DATA, VAR_IO_STORAGE_META_CLASS_DATA_OFFSET);
    uintptr_t io_media_meta_class = xe_slider_kext_slide(io_storage_slider, XE_KEXT_SEGMENT_DATA, VAR_IO_MEDIA_META_CLASS_DATA_OFFSET);
    xe_assert(xe_os_object_is_instance_of(storage, io_storage_meta_class));
    
    uintptr_t children = xe_io_registry_get_children(storage);
    uint32_t count = xe_os_array_count(children);
    
    for (int i=0; i<count; i++) {
        uintptr_t child = xe_os_array_value_at_index(children, i);
        _Bool is_io_media = xe_os_object_is_instance_of(child, io_media_meta_class);
        _Bool is_io_storage = xe_os_object_is_instance_of(child, io_storage_meta_class);
        if (is_io_media) {
            xe_log_debug("patching media client %p", (void*)child);
            _Bool is_writable = xe_kmem_read_uint8(child, TYPE_IO_MEDIA_MEM_IS_WRITABLE_OFFSET);
            if (is_writable) {
                xe_log_warn("media client %p is already writable", (void*)child);
            } else {
                xe_io_registry_set_boolean_prop(child, "Writable", 1);
                xe_kmem_write_uint8(child, TYPE_IO_MEDIA_MEM_IS_WRITABLE_OFFSET, 1);
            }
            if (xe_io_registry_get_boolean_prop(child, "Open") && xe_io_registry_get_boolean_prop(child, "Leaf")) {
                char dev_name[NAME_MAX];
                xe_io_registry_get_string_prop(child, "BSD Name", dev_name, sizeof(dev_name));
                xe_log_debug("updating dev %s mount with rw option", dev_name);
                if (make_dev_mount_writable(dev_name)) {
                    xe_log_error("failed to remount dev %s as writable", dev_name);
                }
            }
        }
        if (is_io_media || is_io_storage) {
            recursive_patch_storage(io_storage_slider, child);
        }
    }
}


int main(int argc, const char * argv[]) {
    const char* kmem_socket = NULL;
    
    int ch;
    while ((ch = getopt(argc, (char**)argv, "k:")) != -1) {
        switch (ch) {
            case 'k': {
                kmem_socket = optarg;
                break;
            }
            case '?':
            default: {
                xe_log_info("usage: demo_wp_disable [-k kmem_uds]");
                exit(1);
            }
        }
    }
    
    xe_init();
    xe_kmem_backend_t backend;
    int error = xe_kmem_remote_client_create(kmem_socket, &backend);
    if (error) {
        xe_log_error("cannot connect to kmem unix domain socket, err: %s", strerror(error));
        exit(1);
    }
    
    xe_kmem_use_backend(backend);
    xe_slider_kernel_init(xe_kmem_remote_client_get_mh_execute_header(backend));
    
    struct xe_io_registry_filter sdxc_slot_path[] = {
        { "name", VALUE_TYPE_STRING, { .string = "device-tree" } },
        { "IOClass", VALUE_TYPE_STRING, { .string = "AppleARMPE" } },
        { "name", VALUE_TYPE_STRING, { .string = "arm-io" } },
        { "IOClass", VALUE_TYPE_STRING, { .string = "AppleT600xIO" } },
        { "name", VALUE_TYPE_STRING, { .string = "apcie" } },
        { "IOClass", VALUE_TYPE_STRING, { .string = "AppleT6000PCIe" } },
        { "IOName", VALUE_TYPE_STRING, { .string = "pci-bridge" } },
        { "IOClass", VALUE_TYPE_STRING, { .string = "IOPCI2PCIBridge" } },
        { "name", VALUE_TYPE_STRING, { .string = "pcie-sdreader" } },
        { "IOClass", VALUE_TYPE_STRING, { .string = "AppleSDXC" } },
        { "IOUnit", VALUE_TYPE_NUMBER, { .number = 0x1 } },
    };
    
    uintptr_t sdxc_slot;
    error = xe_io_registry_search(xe_io_registry_entry_root(), sdxc_slot_path, xe_array_size(sdxc_slot_path), &sdxc_slot);
    if (error) {
        xe_log_error("this computer does not have a supported pcie sdreader");
        exit(1);
    }

    _Bool card_present = xe_io_registry_get_boolean_prop(sdxc_slot, "Card Present");
    if (!card_present) {
        xe_log_error("sd card not present in card reader");
        exit(1);
    }
    
    struct xe_io_registry_filter storage_driver_path[] = {
        { "IOClass", VALUE_TYPE_STRING, { .string = "AppleSDXCBlockStorageDevice" } },
        { "IOClass", VALUE_TYPE_STRING, { .string = "IOBlockStorageDriver" } }
    };
    uintptr_t storage_driver;
    error = xe_io_registry_search(sdxc_slot, storage_driver_path, xe_array_size(storage_driver_path), &storage_driver);
    if (error) {
        xe_log_error("cannot find IOBlockStorageDriver");
        abort();
    }
    
    _Bool write_protected = xe_kmem_read_uint8(storage_driver, TYPE_IO_BLOCK_STORAGE_DRIVER_MEM_WRITE_PROTECTED_OFFSET);
    if (!write_protected) {
        xe_log_error("write protection is already disabled");
    } else {
        xe_log_info("disabling write protection");
        xe_kmem_write_uint8(storage_driver, TYPE_IO_BLOCK_STORAGE_DRIVER_MEM_WRITE_PROTECTED_OFFSET, 0);
        xe_slider_kext_t io_storage_family_slider = xe_slider_kext_create("com.apple.iokit.IOStorageFamily", XE_KC_BOOT);
        xe_log_info("updating mounts with write capability");
        recursive_patch_storage(io_storage_family_slider, storage_driver);
        xe_slider_kext_destroy(&io_storage_family_slider);
    }
    
    return 0;
}
