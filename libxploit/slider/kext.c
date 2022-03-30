
#include <stdlib.h>
#include <string.h>
#include <mach-o/loader.h>

#include "slider/kext.h"
#include "slider/kernel.h"
#include "memory/kmem.h"
#include "util/assert.h"
#include "util/log.h"

#include <macos/kernel.h>


typedef struct {
    uintptr_t start;
    size_t size;
} xpl_slider_kext_segment_info_t;


struct xpl_slider_kext {
    xpl_slider_kext_segment_info_t text;
    xpl_slider_kext_segment_info_t text_exec;
    xpl_slider_kext_segment_info_t data;
    xpl_slider_kext_segment_info_t data_const;
    xpl_slider_kext_segment_info_t link_edit;
};


uintptr_t xpl_slider_kext_find_kext_header(char* identifier, enum xpl_kext_collection_type collection) {
    xpl_assert(collection == XPL_KC_BOOT || collection == XPL_KC_AUX);
    uintptr_t header_location = xpl_kmem_read_uint64(xpl_slider_kernel_slide(collection == XPL_KC_BOOT ? VAR_SEG_LOWEST_KC : VAR_AUXKC_MH), 0);
    
    struct mach_header_64 header;
    xpl_kmem_read(&header, header_location, 0, sizeof(header));
    xpl_assert(header.magic == MH_MAGIC_64);
    
    struct load_command* commands = malloc(header.sizeofcmds);
    xpl_kmem_read(commands, header_location, sizeof(header), header.sizeofcmds);
    
    struct load_command* cursor = commands;
    for (int i=0; i<header.ncmds; i++) {
        xpl_assert((uintptr_t)cursor + cursor->cmdsize <= (uintptr_t)commands + header.sizeofcmds);
        if (cursor->cmd == LC_FILESET_ENTRY) {
            struct fileset_entry_command* fse_command = (struct fileset_entry_command*)cursor;
            xpl_assert(cursor->cmdsize >= sizeof(struct fileset_entry_command));
            char* name = (char*)fse_command + fse_command->entry_id.offset;
            if (strncmp(identifier, name, fse_command->cmdsize - fse_command->entry_id.offset) == 0) {
                uintptr_t vmaddr = fse_command->vmaddr;
                free(commands);
                return vmaddr;
            }
        }
        cursor = (struct load_command*)((uintptr_t)cursor + cursor->cmdsize);
    }
    
    free(commands);
    xpl_log_error("failed to find fileset entry");
    xpl_abort();
}


xpl_slider_kext_t xpl_slider_kext_create(char* identifier, enum xpl_kext_collection_type collection) {
    xpl_slider_kext_t slider = malloc(sizeof(struct xpl_slider_kext));
    
    uintptr_t kext_header_location = xpl_slider_kext_find_kext_header(identifier, collection);
    struct mach_header_64 header;
    xpl_kmem_read(&header, kext_header_location, 0, sizeof(header));
    xpl_assert(header.magic == MH_MAGIC_64);
    
    struct load_command* commands = malloc(header.sizeofcmds);
    xpl_kmem_read(commands, kext_header_location, sizeof(header), header.sizeofcmds);
    
    struct load_command* cursor = commands;
    for (int i=0; i<header.ncmds; i++) {
        xpl_assert((uintptr_t)cursor + cursor->cmdsize <= (uintptr_t)commands + header.sizeofcmds);
        if (cursor->cmd == LC_SEGMENT_64) {
            struct segment_command_64* seg_command = (struct segment_command_64*)cursor;
            xpl_assert(cursor->cmdsize >= sizeof(struct segment_command_64));
            char* name = seg_command->segname;
            
            xpl_slider_kext_segment_info_t info;
            info.start = seg_command->vmaddr;
            info.size = seg_command->vmsize;
            
            if (strcmp(name, "__TEXT") == 0) {
                slider->text = info;
            } else if (strcmp(name, "__TEXT_EXEC") == 0) {
                slider->text_exec = info;
            } else if (strcmp(name, "__DATA") == 0) {
                slider->data = info;
            } else if (strcmp(name, "__DATA_CONST") == 0) {
                slider->data_const = info;
            } else if (strcmp(name, "__LINKEDIT") == 0) {
                slider->link_edit = info;
            }
        }
        cursor = (struct load_command*)((uintptr_t)cursor + cursor->cmdsize);
    }
    
    free(commands);
    return slider;
}


uintptr_t xpl_slider_kext_slide(xpl_slider_kext_t slider, enum xpl_kext_segment segment, uintptr_t offset) {
    switch (segment) {
        case XPL_KEXT_SEGMENT_TEXT:
            xpl_assert(offset <= slider->text.size);
            return slider->text.start + offset;
        case XPL_KEXT_SEGMENT_TEXT_EXEC:
            xpl_assert(offset <= slider->text_exec.size);
            return slider->text_exec.start + offset;
        case XPL_KEXT_SEGMENT_DATA_CONST:
            xpl_assert(offset <= slider->data_const.size);
            return slider->data_const.start + offset;
        case XPL_KEXT_SEGMENT_DATA:
            xpl_assert(offset <= slider->data.size);
            return slider->data.start + offset;
        case XPL_KEXT_SEGMENT_LINK_EDIT:
            xpl_assert(offset <= slider->link_edit.size);
            return slider->link_edit.start + offset;
        default:
            xpl_log_error("unknown segment");
            xpl_abort();
    }
}


#define IS_ADDRESS_IN_SEGMENT(addr, info) (addr >= info.start && addr < info.start + info.size)


uintptr_t xpl_slider_kext_unslide(xpl_slider_kext_t slider, enum xpl_kext_segment segment, uintptr_t address) {
    switch (segment) {
        case XPL_KEXT_SEGMENT_TEXT:
            xpl_assert(IS_ADDRESS_IN_SEGMENT(address, slider->text));
            return address - slider->text.start;
        case XPL_KEXT_SEGMENT_TEXT_EXEC:
            xpl_assert(IS_ADDRESS_IN_SEGMENT(address, slider->text_exec));
            return address - slider->text_exec.start;
        case XPL_KEXT_SEGMENT_DATA:
            xpl_assert(IS_ADDRESS_IN_SEGMENT(address, slider->data));
            return address - slider->data.start;
        case XPL_KEXT_SEGMENT_DATA_CONST:
            xpl_assert(IS_ADDRESS_IN_SEGMENT(address, slider->data_const));
            return address - slider->data_const.start;
        case XPL_KEXT_SEGMENT_LINK_EDIT:
            xpl_assert(IS_ADDRESS_IN_SEGMENT(address, slider->link_edit));
            return address - slider->link_edit.start;
        default:
            xpl_log_error("unknown segment");
            xpl_abort();
    }
}


void xpl_slider_kext_destroy(xpl_slider_kext_t* slider_p) {
    free(*slider_p);
    *slider_p = NULL;
}
