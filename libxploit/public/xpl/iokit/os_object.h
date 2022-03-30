
#ifndef xpl_os_object_h
#define xpl_os_object_h

#include <stdio.h>

uintptr_t xpl_os_object_get_meta_class(uintptr_t instance);
_Bool xpl_os_object_is_instance_of(uintptr_t instance, uintptr_t meta_class);

#endif /* xpl_os_object_h */
