/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * FILE_ID: thread_status.h
 */


#ifndef _MACOS_ARM_THREAD_STATUS_H_
#define _MACOS_ARM_THREAD_STATUS_H_


struct arm_state_hdr64 {
    uint32_t flavor;
    uint32_t count;
};

struct arm_saved_state64 {
    uint64_t x[29];     /* General purpose registers x0-x28 */
    uint64_t fp;        /* Frame pointer x29 */
    uint64_t lr;        /* Link register x30 */
    uint64_t sp;        /* Stack pointer x31 */
    uint64_t pc;        /* Program counter */
    uint32_t cpsr;      /* Current program status register */
    uint32_t aspr;      /* Reserved padding */
    uint64_t far;       /* Virtual fault address */
    uint32_t esr;       /* Exception syndrome register */
    uint32_t exception; /* Exception number */
    uint64_t jophash;
};

struct arm_saved_state {
    struct arm_state_hdr64 ash;
    struct arm_saved_state64 uss;
};

typedef __uint128_t uint128_t;
typedef uint64_t uint64x2_t __attribute__((ext_vector_type(2)));
typedef uint32_t uint32x4_t __attribute__((ext_vector_type(4)));

struct arm_neon_saved_state64 {
    union {
        uint128_t  q[32];
        uint64x2_t d[32];
        uint32x4_t s[32];
    } v;
    uint32_t fpsr;
    uint32_t fpcr;
};

struct arm_neon_saved_state {
    struct arm_state_hdr64 nsh;
    struct arm_neon_saved_state64 uns;
};

struct arm_context {
    struct arm_saved_state ss;
    struct arm_neon_saved_state ns;
};

#endif /* _MACOS_ARM_THREAD_STATUS_H_ */

