//
//  slider_kext.c
//  libxe
//
//  Created by admin on 1/1/22.
//

#include <stdlib.h>
#include <string.h>
#include <mach-o/loader.h>

#include "slider_kext.h"
#include "kmem.h"
#include "slider.h"
#include "platform_params.h"
#include "util_assert.h"


typedef struct {
    uintptr_t start;
    size_t size;
} xe_slider_kext_segment_info_t;


struct xe_slider_kext {
    xe_slider_kext_segment_info_t text;
    xe_slider_kext_segment_info_t text_exec;
    xe_slider_kext_segment_info_t data;
    xe_slider_kext_segment_info_t data_const;
    xe_slider_kext_segment_info_t link_edit;
};


uintptr_t xe_slider_kext_find_kext_header(char* identifier, enum xe_kext_collection_type collection) {
    xe_assert(collection == XE_KC_BOOT || collection == XE_KC_AUX);
    uintptr_t header_location = xe_kmem_read_uint64(xe_slider_slide(collection == XE_KC_BOOT ? VAR_SEG_LOWEST_KC : VAR_AUXKC_MH));
    
    struct mach_header_64 header;
    xe_kmem_read(&header, header_location, sizeof(header));
    xe_assert(header.magic == MH_MAGIC_64);
    
    struct load_command* commands = malloc(header.sizeofcmds);
    xe_kmem_read(commands, header_location + sizeof(header), header.sizeofcmds);
    
    struct load_command* cursor = commands;
    for (int i=0; i<header.ncmds; i++) {
        xe_assert((uintptr_t)cursor + cursor->cmdsize <= (uintptr_t)commands + header.sizeofcmds);
        if (cursor->cmd == LC_FILESET_ENTRY) {
            struct fileset_entry_command* fse_command = (struct fileset_entry_command*)cursor;
            xe_assert(cursor->cmdsize >= sizeof(struct fileset_entry_command));
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
    printf("[ERROR] failed to find fileset entry\n");
    abort();
}


xe_slider_kext_t xe_slider_kext_create(char* identifier, enum xe_kext_collection_type collection) {
    xe_slider_kext_t slider = malloc(sizeof(struct xe_slider_kext));
    
    uintptr_t kext_header_location = xe_slider_kext_find_kext_header(identifier, collection);
    struct mach_header_64 header;
    xe_kmem_read(&header, kext_header_location, sizeof(header));
    xe_assert(header.magic == MH_MAGIC_64);
    
    struct load_command* commands = malloc(header.sizeofcmds);
    xe_kmem_read(commands, kext_header_location + sizeof(header), header.sizeofcmds);
    
    struct load_command* cursor = commands;
    for (int i=0; i<header.ncmds; i++) {
        xe_assert((uintptr_t)cursor + cursor->cmdsize <= (uintptr_t)commands + header.sizeofcmds);
        if (cursor->cmd == LC_SEGMENT_64) {
            struct segment_command_64* seg_command = (struct segment_command_64*)cursor;
            xe_assert(cursor->cmdsize >= sizeof(struct segment_command_64));
            char* name = seg_command->segname;
            
            xe_slider_kext_segment_info_t info;
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


uintptr_t xe_slider_kext_slide(xe_slider_kext_t slider, enum xe_kext_segment segment, uintptr_t offset) {
    switch (segment) {
        case XE_KEXT_SEGMENT_TEXT:
            xe_assert(offset <= slider->text.size);
            return slider->text.start + offset;
        case XE_KEXT_SEGMENT_TEXT_EXEC:
            xe_assert(offset <= slider->text_exec.size);
            return slider->text_exec.start + offset;
        case XE_KEXT_SEGMENT_DATA_CONST:
            xe_assert(offset <= slider->data_const.size);
            return slider->data_const.start + offset;
        case XE_KEXT_SEGMENT_DATA:
            xe_assert(offset <= slider->data.size);
            return slider->data.start + offset;
        case XE_KEXT_SEGMENT_LINK_EDIT:
            xe_assert(offset <= slider->link_edit.size);
            return slider->link_edit.start + offset;
        default:
            printf("[ERROR] unknown segment\n");
            abort();
    }
}


#define IS_ADDRESS_IN_SEGMENT(addr, info) (addr >= info.start && addr < info.start + info.size)


uintptr_t xe_slider_kext_unslide(xe_slider_kext_t slider, enum xe_kext_segment segment, uintptr_t address) {
    switch (segment) {
        case XE_KEXT_SEGMENT_TEXT:
            xe_assert(IS_ADDRESS_IN_SEGMENT(address, slider->text));
            return address - slider->text.start;
        case XE_KEXT_SEGMENT_TEXT_EXEC:
            xe_assert(IS_ADDRESS_IN_SEGMENT(address, slider->text_exec));
            return address - slider->text_exec.start;
        case XE_KEXT_SEGMENT_DATA:
            xe_assert(IS_ADDRESS_IN_SEGMENT(address, slider->data));
            return address - slider->data.start;
        case XE_KEXT_SEGMENT_DATA_CONST:
            xe_assert(IS_ADDRESS_IN_SEGMENT(address, slider->data_const));
            return address - slider->data_const.start;
        case XE_KEXT_SEGMENT_LINK_EDIT:
            xe_assert(IS_ADDRESS_IN_SEGMENT(address, slider->link_edit));
            return address - slider->link_edit.start;
        default:
            printf("[ERROR] unknown segment\n");
            abort();
    }
}


void xe_slider_kext_destroy(xe_slider_kext_t* slider_p) {
    free(*slider_p);
    *slider_p = NULL;
}
