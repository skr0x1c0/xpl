
#include <stdlib.h>
#include <string.h>

#include <mach-o/loader.h>

#include "slider/kernel.h"
#include "memory/kmem.h"
#include "util/assert.h"
#include "util/log.h"

#include <macos/kernel.h>


typedef struct segment_info {
    uintptr_t base;
    size_t size;
} segment_info_t;


struct xpl_slider_segment_infos {
    segment_info_t text;
    segment_info_t data_const;
    segment_info_t text_exec;
    segment_info_t kld;
    segment_info_t ppl_text;
    segment_info_t ppl_data_const;
    segment_info_t last_data_const;
    segment_info_t last;
    segment_info_t ppl_data;
    segment_info_t kld_data;
    segment_info_t data;
    segment_info_t hib_data;
    segment_info_t boot_data;
};

#define SEGMENT_INFO(seg) { XPL_IMAGE_SEGMENT_##seg##_BASE, XPL_IMAGE_SEGMENT_##seg##_SIZE }

static struct xpl_slider_segment_infos xpl_slider_segment_infos_image = {
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


static struct xpl_slider_segment_infos xpl_slider_segment_infos_kern;


void xpl_slider_kernel_init(uintptr_t mh_execute_header) {
    struct mach_header_64 header;
    xpl_kmem_read(&header, mh_execute_header, 0, sizeof(header));
    xpl_assert(header.magic == MH_MAGIC_64);
    
    struct load_command* commands = malloc(header.sizeofcmds);
    xpl_kmem_read(commands, mh_execute_header, sizeof(header), header.sizeofcmds);
    
    struct load_command* cursor = commands;
    for (int i=0; i<header.ncmds; i++) {
        xpl_assert((uintptr_t)cursor + cursor->cmdsize <= (uintptr_t)commands + header.sizeofcmds);
        if (cursor->cmd == LC_SEGMENT_64) {
            xpl_assert(cursor->cmdsize >= sizeof(struct segment_command_64));
            struct segment_command_64* seg_command = (struct segment_command_64*)cursor;
            char* seg_name = seg_command->segname;
            if (strcmp(seg_name, "__TEXT") == 0) {
                xpl_slider_segment_infos_kern.text.base = seg_command->vmaddr;
                xpl_slider_segment_infos_kern.text.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__DATA_CONST") == 0) {
                xpl_slider_segment_infos_kern.data_const.base = seg_command->vmaddr;
                xpl_slider_segment_infos_kern.data_const.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__TEXT_EXEC") == 0) {
                xpl_slider_segment_infos_kern.text_exec.base = seg_command->vmaddr;
                xpl_slider_segment_infos_kern.text_exec.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__KLD") == 0) {
                xpl_slider_segment_infos_kern.kld.base = seg_command->vmaddr;
                xpl_slider_segment_infos_kern.kld.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__PPLTEXT") == 0) {
                xpl_slider_segment_infos_kern.ppl_text.base = seg_command->vmaddr;
                xpl_slider_segment_infos_kern.ppl_text.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__PPLDATA_CONST") == 0) {
                xpl_slider_segment_infos_kern.ppl_data_const.base = seg_command->vmaddr;
                xpl_slider_segment_infos_kern.ppl_data_const.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__LASTDATA_CONST") == 0) {
                xpl_slider_segment_infos_kern.last_data_const.base = seg_command->vmaddr;
                xpl_slider_segment_infos_kern.last_data_const.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__LAST") == 0) {
                xpl_slider_segment_infos_kern.last.base = seg_command->vmaddr;
                xpl_slider_segment_infos_kern.last.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__KLDDATA") == 0) {
                xpl_slider_segment_infos_kern.kld_data.base = seg_command->vmaddr;
                xpl_slider_segment_infos_kern.kld_data.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__DATA") == 0) {
                xpl_slider_segment_infos_kern.data.base = seg_command->vmaddr;
                xpl_slider_segment_infos_kern.data.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__HIBDATA") == 0) {
                xpl_slider_segment_infos_kern.hib_data.base = seg_command->vmaddr;
                xpl_slider_segment_infos_kern.hib_data.size = seg_command->vmsize;
            } else if (strcmp(seg_name, "__BOOTDATA") == 0) {
                xpl_slider_segment_infos_kern.boot_data.base = seg_command->vmaddr;
                xpl_slider_segment_infos_kern.boot_data.size = seg_command->vmsize;
            }
        }
        cursor = (struct load_command*)((uintptr_t)cursor + cursor->cmdsize);
    }
    
    free(commands);
}

#define is_addr_in(addr, infos, segment) (addr >= infos->segment.base && addr < infos->segment.base + infos->segment.size)
#define slide(addr, from, to, segment) (addr + ((int64_t)to->segment.base - (int64_t)from->segment.base))

uintptr_t xpl_slider_kernel_slide_internal(uintptr_t address, struct xpl_slider_segment_infos* to, struct xpl_slider_segment_infos* from) {
    if (is_addr_in(address, from, text)) {
        return slide(address, from, to, text);
    } else if (is_addr_in(address, from, data_const)) {
        return slide(address, from, to, data_const);
    } else if (is_addr_in(address, from, text_exec)) {
        return slide(address, from, to, text_exec);
    } else if (is_addr_in(address, from, kld)) {
        return slide(address, from, to, kld);
    } else if (is_addr_in(address, from, ppl_text)) {
        return slide(address, from, to, ppl_text);
    } else if (is_addr_in(address, from, ppl_data_const)) {
        return slide(address, from, to, ppl_data_const);
    } else if (is_addr_in(address, from, last_data_const)) {
        return slide(address, from, to, last_data_const);
    } else if (is_addr_in(address, from, last)) {
        return slide(address, from, to, last);
    } else if (is_addr_in(address, from, ppl_data)) {
        return slide(address, from, to, ppl_data);
    } else if (is_addr_in(address, from, kld_data)) {
        return slide(address, from, to, kld_data);
    } else if (is_addr_in(address, from, data)) {
        return slide(address, from, to, data);
    } else if (is_addr_in(address, from, hib_data)) {
        return slide(address, from, to, hib_data);
    } else if (is_addr_in(address, from, boot_data)) {
        return slide(address, from, to, boot_data);
    }
    
    xpl_log_error("address does not belong to any known segment");
    xpl_abort();
}

uintptr_t xpl_slider_kernel_slide(uintptr_t address) {
    return xpl_slider_kernel_slide_internal(address, &xpl_slider_segment_infos_kern, &xpl_slider_segment_infos_image);
}

uintptr_t xpl_slider_kernel_unslide(uintptr_t address) {
    return xpl_slider_kernel_slide_internal(address, &xpl_slider_segment_infos_image, &xpl_slider_segment_infos_kern);
}
