//
//  slider_kext.h
//  libxe
//
//  Created by admin on 1/1/22.
//

#ifndef xe_slider_kext_h
#define xe_slider_kext_h

#include <stdio.h>

typedef struct xe_slider_kext* xe_slider_kext_t;

enum xe_kext_collection_type {
    XE_KC_BOOT,
    XE_KC_AUX
};

enum xe_kext_segment {
    XE_KEXT_SEGMENT_TEXT,
    XE_KEXT_SEGMENT_TEXT_EXEC,
    XE_KEXT_SEGMENT_DATA,
    XE_KEXT_SEGMENT_DATA_CONST,
    XE_KEXT_SEGMENT_LINK_EDIT
};


xe_slider_kext_t xe_slider_kext_create(char* identifier, enum xe_kext_collection_type collection);
uintptr_t xe_slider_kext_slide(xe_slider_kext_t slider, enum xe_kext_segment segment, uintptr_t offset);
uintptr_t xe_slider_kext_unslide(xe_slider_kext_t slider, enum xe_kext_segment segment, uintptr_t address);
void xe_slider_kext_destroy(xe_slider_kext_t* slider_p);


#endif /* xe_slider_kext_h */
