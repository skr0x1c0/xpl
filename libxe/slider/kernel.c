//
//  slider.c
//  libxe
//
//  Created by admin on 1/1/22.
//

#include <stdlib.h>
#include <string.h>

#include <mach-o/loader.h>

#include "slider/kernel.h"
#include "memory/kmem.h"
#include "util/assert.h"
#include "util/log.h"

#include "macos_params.h"


typedef struct xe_slider_segment_info {
    uintptr_t base;
    size_t size;
} xe_slider_segment_info_t;


struct xe_slider_segment_infos {
    xe_slider_segment_info_t text;
    xe_slider_segment_info_t data_const;
    xe_slider_segment_info_t text_exec;
    xe_slider_segment_info_t kld;
    xe_slider_segment_info_t ppl_text;
    xe_slider_segment_info_t ppl_data_const;
    xe_slider_segment_info_t last_data_const;
    xe_slider_segment_info_t last;
    xe_slider_segment_info_t ppl_data;
    xe_slider_segment_info_t kld_data;
    xe_slider_segment_info_t data;
    xe_slider_segment_info_t hib_data;
    xe_slider_segment_info_t boot_data;
};

#define SEGMENT_INFO(seg) { XE_IMAGE_SEGMENT_##seg##_BASE, XE_IMAGE_SEGMENT_##seg##_SIZE }

static struct xe_slider_segment_infos xe_slider_segment_infos_image = {
    .text = SEGMENT_INFO(TEXT),
    .data_const = SEGMENT_INFO(DATA_CONST),
    .text_exec = SEGMENT_INFO(TEXT_EXEC),
    .kld = SEGMENT_INFO(KLD),
    .ppl_text = SEGMENT_INFO(PPL_TEXT),
    .ppl_data_const = SEGMENT_INFO(PPL_DATA_CONST),
    .last_data_const = SEGMENT_INFO(LAST_DATA_CONST),
    .last = SEGMENT_INFO(LAST),
    .ppl_data = SEGMENT_INFO(PPL_DATA),
    .kld_data = SEGMENT_INFO(KLD_DATA),
    .data = SEGMENT_INFO(DATA),
    .hib_data = SEGMENT_INFO(HIB_DATA),
    .boot_data = SEGMENT_INFO(BOOT_DATA),
};


static struct xe_slider_segment_infos xe_slider_segment_infos_kern;


void xe_slider_kernel_init(uintptr_t mh_execute_header) {
    struct mach_header_64 header;
    xe_kmem_read(&header, mh_execute_header, 0, sizeof(header));
    xe_assert(header.magic == MH_MAGIC_64);
    
    struct load_command* commands = malloc(header.sizeofcmds);
    xe_kmem_read(commands, mh_execute_header, sizeof(header), header.sizeofcmds);
    
    struct load_command* cursor = commands;
    for (int i=0; i<header.ncmds; i++) {
        xe_assert((uintptr_t)cursor + cursor->cmdsize <= (uintptr_t)commands + header.sizeofcmds);
        if (cursor->cmd == LC_SEGMENT_64) {
            xe_assert(cursor->cmdsize >= sizeof(struct segment_command_64));
            struct segment_command_64* seg_command = (struct segment_command_64*)cursor;
            char* seg_name = seg_command->segname;
            if (strcmp(seg_name, "__TEXT") == 0) {
                xe_slider_segment_infos_kern.text.base = seg_command->vmaddr;
                xe_slider_segment_infos_kern.text.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__DATA_CONST") == 0) {
                xe_slider_segment_infos_kern.data_const.base = seg_command->vmaddr;
                xe_slider_segment_infos_kern.data_const.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__TEXT_EXEC") == 0) {
                xe_slider_segment_infos_kern.text_exec.base = seg_command->vmaddr;
                xe_slider_segment_infos_kern.text_exec.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__KLD") == 0) {
                xe_slider_segment_infos_kern.kld.base = seg_command->vmaddr;
                xe_slider_segment_infos_kern.kld.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__PPLTEXT") == 0) {
                xe_slider_segment_infos_kern.ppl_text.base = seg_command->vmaddr;
                xe_slider_segment_infos_kern.ppl_text.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__PPLDATA_CONST") == 0) {
                xe_slider_segment_infos_kern.ppl_data_const.base = seg_command->vmaddr;
                xe_slider_segment_infos_kern.ppl_data_const.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__LASTDATA_CONST") == 0) {
                xe_slider_segment_infos_kern.last_data_const.base = seg_command->vmaddr;
                xe_slider_segment_infos_kern.last_data_const.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__LAST") == 0) {
                xe_slider_segment_infos_kern.last.base = seg_command->vmaddr;
                xe_slider_segment_infos_kern.last.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__KLDDATA") == 0) {
                xe_slider_segment_infos_kern.kld_data.base = seg_command->vmaddr;
                xe_slider_segment_infos_kern.kld_data.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__DATA") == 0) {
                xe_slider_segment_infos_kern.data.base = seg_command->vmaddr;
                xe_slider_segment_infos_kern.data.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__HIBDATA") == 0) {
                xe_slider_segment_infos_kern.hib_data.base = seg_command->vmaddr;
                xe_slider_segment_infos_kern.hib_data.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__BOOTDATA") == 0) {
                xe_slider_segment_infos_kern.boot_data.base = seg_command->vmaddr;
                xe_slider_segment_infos_kern.boot_data.size = seg_command->vmsize;
            }
        }
        cursor = (struct load_command*)((uintptr_t)cursor + cursor->cmdsize);
    }
    
    free(commands);
}

#define IS_ADDR_IN(addr, infos, segment) (addr >= infos->segment.base && addr < infos->segment.base + infos->segment.size)
#define SLIDE(addr, from, to, segment) (addr + ((int64_t)to->segment.base - (int64_t)from->segment.base))

uintptr_t xe_slider_kernel_slide_internal(uintptr_t address, struct xe_slider_segment_infos* to, struct xe_slider_segment_infos* from) {
    if (IS_ADDR_IN(address, from, text)) {
        return SLIDE(address, from, to, text);
    } else if (IS_ADDR_IN(address, from, data_const)) {
        return SLIDE(address, from, to, data_const);
    } else if (IS_ADDR_IN(address, from, text_exec)) {
        return SLIDE(address, from, to, text_exec);
    } else if (IS_ADDR_IN(address, from, kld)) {
        return SLIDE(address, from, to, kld);
    } else if (IS_ADDR_IN(address, from, ppl_text)) {
        return SLIDE(address, from, to, ppl_text);
    } else if (IS_ADDR_IN(address, from, ppl_data_const)) {
        return SLIDE(address, from, to, ppl_data_const);
    } else if (IS_ADDR_IN(address, from, last_data_const)) {
        return SLIDE(address, from, to, last_data_const);
    } else if (IS_ADDR_IN(address, from, last)) {
        return SLIDE(address, from, to, last);
    } else if (IS_ADDR_IN(address, from, ppl_data)) {
        return SLIDE(address, from, to, ppl_data);
    } else if (IS_ADDR_IN(address, from, kld_data)) {
        return SLIDE(address, from, to, kld_data);
    } else if (IS_ADDR_IN(address, from, data)) {
        return SLIDE(address, from, to, data);
    } else if (IS_ADDR_IN(address, from, hib_data)) {
        return SLIDE(address, from, to, hib_data);
    } else if (IS_ADDR_IN(address, from, boot_data)) {
        return SLIDE(address, from, to, boot_data);
    }
    
    xe_log_error("address does not belong to any known segment");
    abort();
}

uintptr_t xe_slider_kernel_slide(uintptr_t address) {
    return xe_slider_kernel_slide_internal(address, &xe_slider_segment_infos_kern, &xe_slider_segment_infos_image);
}

uintptr_t xe_slider_kernel_unslide(uintptr_t address) {
    return xe_slider_kernel_slide_internal(address, &xe_slider_segment_infos_image, &xe_slider_segment_infos_kern);
}
