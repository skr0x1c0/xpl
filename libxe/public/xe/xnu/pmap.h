//
//  pmap.h
//  libxe
//
//  Created by sreejith on 3/8/22.
//

#ifndef pmap_h
#define pmap_h

#include <stdio.h>

uintptr_t xe_xnu_pmap_phystokv(uintptr_t pa);
uintptr_t xe_xnu_pmap_kvtophys(uintptr_t pmap, uintptr_t va);

#endif /* pmap_h */
