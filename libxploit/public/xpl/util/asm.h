//
//  assembly.h
//  libxe
//
//  Created by sreejith on 3/14/22.
//

#ifndef xpl_assembly_h
#define xpl_assembly_h

#include <stdio.h>

/// Constructs aarch64 bl instruction
/// @param to address to branch to
/// @param from address of instruction
uint32_t xpl_asm_build_bl_instr(uintptr_t to, uintptr_t from);

#endif /* xpl_assembly_h */
