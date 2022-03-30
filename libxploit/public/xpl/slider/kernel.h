
#ifndef xpl_slider_h
#define xpl_slider_h

#include <stdio.h>

void xpl_slider_kernel_init(uintptr_t mh_execute_header);
uintptr_t xpl_slider_kernel_slide(uintptr_t address);
uintptr_t xpl_slider_kernel_unslide(uintptr_t address);

#endif /* xpl_slider_h */
