//
//  os_object.h
//  libxe
//
//  Created by sreejith on 3/10/22.
//

#ifndef os_object_h
#define os_object_h

#include <stdio.h>

uintptr_t xpl_os_object_get_meta_class(uintptr_t instance);
_Bool xpl_os_object_is_instance_of(uintptr_t instance, uintptr_t meta_class);

#endif /* os_object_h */
