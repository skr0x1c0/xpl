//
//  cp.h
//  libxe
//
//  Created by sreejith on 2/24/22.
//

#ifndef cp_h
#define cp_h

#include <stdio.h>

/// Copy file from path `src` to `dst`. File in path `dst` should not exist
/// @param src source file
/// @param dst destination file
int xpl_cp(const char* src, const char* dst);

#endif /* cp_h */
