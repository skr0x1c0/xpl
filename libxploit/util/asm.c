//
//  assembly.c
//  libxe
//
//  Created by sreejith on 3/14/22.
//

#include "util/asm.h"
#include "util/assert.h"


uint32_t xpl_util_asm_build_bl_instr(uintptr_t to, uintptr_t from) {
    int64_t offset = to - from;
    xpl_assert_cond(offset % 4, ==, 0);
    uint32_t imm6 = (offset >> 2) & 0x3FFFFFF;
    return imm6 | 0x94000000;
}
