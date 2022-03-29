//
//  main.c
//  demo_wp_disable
//
//  Created by admin on 2/18/22.
//


#include <stdio.h>
#include <limits.h>

#include <xpl/xpl.h>
#include <xpl/memory/kmem.h>
#include <xpl/memory/kmem_remote.h>
#include <xpl/slider/kernel.h>
#include <xpl/slider/kext.h>
#include <xpl/iokit/io_registry_entry.h>
#include <xpl/iokit/os_object.h>
#include <xpl/iokit/os_array.h>
#include <xpl/iokit/os_dictionary.h>
#include <xpl/util/log.h>
#include <xpl/util/assert.h>
#include <xpl/util/misc.h>

#define VAR_IO_MEDIA_META_CLASS_DATA_OFFSET 0x310
#define VAR_IO_STORAGE_META_CLASS_DATA_OFFSET 0x388


int make_dev_mount_writable(const char* dev) {
    char cmd[PATH_MAX];
    snprintf(cmd, sizeof(cmd), "diskutil mount -mountOptions \"rw,update\" /dev/%s", dev);
    
    int result = system(cmd);
    if (result == 0) {
        return 0;
    }
    
    xpl_log_warn("update mount with rw failed, trying remount");
    
    snprintf(cmd, sizeof(cmd), "diskutil unmount /dev/%s", dev);
    result = system(cmd);
    if (result != 0) {
        xpl_log_error("diskutil unmount failed, exit code: %d", result);
        return EBADF;
    }
    
    snprintf(cmd, sizeof(cmd), "diskutil mount -mountOptions \"rw\" /dev/%s", dev);
    result = system(cmd);
    if (result != 0) {
        xpl_log_error("diskutil mount failed, exit code: %d", result);
        return EBADF;
    }
    
    return 0;
}


void recursive_patch_storage(xpl_slider_kext_t io_storage_slider, uintptr_t storage) {
    uintptr_t io_storage_meta_class = xpl_slider_kext_slide(io_storage_slider, xpl_KEXT_SEGMENT_DATA, VAR_IO_STORAGE_META_CLASS_DATA_OFFSET);
    uintptr_t io_media_meta_class = xpl_slider_kext_slide(io_storage_slider, xpl_KEXT_SEGMENT_DATA, VAR_IO_MEDIA_META_CLASS_DATA_OFFSET);
    xpl_assert(xpl_os_object_is_instance_of(storage, io_storage_meta_class));
    
    uintptr_t children = xpl_io_registry_get_children(storage);
    uint32_t count = xpl_os_array_count(children);
    
    for (int i=0; i<count; i++) {
        uintptr_t child = xpl_os_array_value_at_index(children, i);
        _Bool is_io_media = xpl_os_object_is_instance_of(child, io_media_meta_class);
        _Bool is_io_storage = xpl_os_object_is_instance_of(child, io_storage_meta_class);
        if (is_io_media) {
            xpl_log_debug("patching media client %p", (void*)child);
            _Bool is_writable = xpl_kmem_read_uint8(child, TYPE_IO_MEDIA_MEM_IS_WRITABLE_OFFSET);
            if (is_writable) {
                xpl_log_warn("media client %p is already writable", (void*)child);
            } else {
                xpl_io_registry_set_boolean_prop(child, "Writable", 1);
                xpl_kmem_write_uint8(child, TYPE_IO_MEDIA_MEM_IS_WRITABLE_OFFSET, 1);
            }
            if (xpl_io_registry_get_boolean_prop(child, "Open") && xpl_io_registry_get_boolean_prop(child, "Leaf")) {
                char dev_name[NAME_MAX];
                xpl_io_registry_get_string_prop(child, "BSD Name", dev_name, sizeof(dev_name));
                xpl_log_debug("updating dev %s mount with rw option", dev_name);
                if (make_dev_mount_writable(dev_name)) {
                    xpl_log_error("failed to remount dev %s as writable", dev_name);
                }
            }
        }
        if (is_io_media || is_io_storage) {
            recursive_patch_storage(io_storage_slider, child);
        }
    }
}


int main(int argc, const char * argv[]) {
    const char* kmem_socket = xpl_DEFAULT_KMEM_SOCKET;
    
    int ch;
    while ((ch = getopt(argc, (char**)argv, "k:")) != -1) {
        switch (ch) {
            case 'k': {
                kmem_socket = optarg;
                break;
            }
            case '?':
            default: {
                xpl_log_info("usage: demo_wp_disable [-k kmem_uds]");
                exit(1);
            }
        }
    }
    
    xpl_init();
    xpl_kmem_backend_t backend;
    int error = xpl_kmem_remote_client_create(kmem_socket, &backend);
    if (error) {
        xpl_log_error("cannot connect to kmem unix domain socket %s, err: %s", kmem_socket, strerror(error));
        exit(1);
    }
    
    xpl_kmem_use_backend(backend);
    xpl_slider_kernel_init(xpl_kmem_remote_client_get_mh_execute_header(backend));
    
    struct xpl_io_registry_filter sdxc_slot_path[] = {
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
    error = xpl_io_registry_search(xpl_io_registry_entry_root(), sdxc_slot_path, xpl_array_size(sdxc_slot_path), &sdxc_slot);
    if (error) {
        xpl_log_error("this computer does not have a supported pcie sdreader");
        exit(1);
    }

    _Bool card_present = xpl_io_registry_get_boolean_prop(sdxc_slot, "Card Present");
    if (!card_present) {
        xpl_log_error("sd card not present in card reader");
        exit(1);
    }
    
    struct xpl_io_registry_filter storage_driver_path[] = {
        { "IOClass", VALUE_TYPE_STRING, { .string = "AppleSDXCBlockStorageDevice" } },
        { "IOClass", VALUE_TYPE_STRING, { .string = "IOBlockStorageDriver" } }
    };
    uintptr_t storage_driver;
    error = xpl_io_registry_search(sdxc_slot, storage_driver_path, xpl_array_size(storage_driver_path), &storage_driver);
    if (error) {
        xpl_log_error("cannot find IOBlockStorageDriver");
        abort();
    }
    
    _Bool write_protected = xpl_kmem_read_uint8(storage_driver, TYPE_IO_BLOCK_STORAGE_DRIVER_MEM_WRITE_PROTECTED_OFFSET);
    if (!write_protected) {
        xpl_log_error("write protection is already disabled");
    } else {
        xpl_log_info("disabling write protection");
        xpl_kmem_write_uint8(storage_driver, TYPE_IO_BLOCK_STORAGE_DRIVER_MEM_WRITE_PROTECTED_OFFSET, 0);
        xpl_slider_kext_t io_storage_family_slider = xpl_slider_kext_create("com.apple.iokit.IOStorageFamily", xpl_KC_BOOT);
        xpl_log_info("updating mounts with write capability");
        recursive_patch_storage(io_storage_family_slider, storage_driver);
        xpl_slider_kext_destroy(&io_storage_family_slider);
    }
    
    return 0;
}
