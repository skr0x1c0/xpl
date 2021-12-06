//
//  platform_constants.h
//  xe
//
//  Created by admin on 12/5/21.
//

#ifndef platform_constants_h
#define platform_constants_h

#define XE_PAGE_SIZE 16384
#define XE_PAGE_SHIFT 14

#define xe_ptoa(x) ((uint64_t)(x) << XE_PAGE_SHIFT)
#define xe_atop(x) ((uint64_t)(x) >> XE_PAGE_SHIFT)

#endif /* platform_constants_h */
