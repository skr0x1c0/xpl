//
//  slider.h
//  xe
//
//  Created by admin on 11/25/21.
//

#ifndef slider_h
#define slider_h

#include <stdio.h>

void xe_slider_init(void);
uintptr_t xe_slider_slide(uintptr_t address);
uintptr_t xe_slider_unslide(uintptr_t address);

#endif /* slider_h */