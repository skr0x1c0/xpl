//
//  cmd_hdiutil.h
//  xe
//
//  Created by admin on 11/26/21.
//

#ifndef xe_cmd_hdiutil_h
#define xe_cmd_hdiutil_h

#include <stdio.h>

int xe_cmd_hdiutil_attach(const char* image, const char* opts[], size_t num_opts, char* dev_out, size_t dev_size);
int xe_cmd_hdiutil_detach(const char* dev_path);

#endif /* xe_cmd_hdiutil_h */
