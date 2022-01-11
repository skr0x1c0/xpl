//
//  utils_misc.h
//  xe
//
//  Created by admin on 11/26/21.
//

#ifndef xe_util_misc_h
#define xe_util_misc_h

#define XE_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define XE_MIN(a, b) ((a) < (b) ? (a) : (b))
#define XE_MAX(a, b) ((a) > (b) ? (a) : (b))

#define XE_BITFIELD_MASK(off, size) (((1ULL << (size)) - 1) << (off))

#endif /* xe_util_misc_h */
