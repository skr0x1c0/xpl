
#ifndef xpl_slider_kext_h
#define xpl_slider_kext_h

#include <stdio.h>

typedef struct xpl_slider_kext* xpl_slider_kext_t;

enum xpl_kext_collection_type {
    XPL_KC_BOOT,
    XPL_KC_AUX
};

enum xpl_kext_segment {
    XPL_KEXT_SEGMENT_TEXT,
    XPL_KEXT_SEGMENT_TEXT_EXEC,
    XPL_KEXT_SEGMENT_DATA,
    XPL_KEXT_SEGMENT_DATA_CONST,
    XPL_KEXT_SEGMENT_LINK_EDIT
};


xpl_slider_kext_t xpl_slider_kext_create(char* identifier, enum xpl_kext_collection_type collection);
uintptr_t xpl_slider_kext_slide(xpl_slider_kext_t slider, enum xpl_kext_segment segment, uintptr_t offset);
uintptr_t xpl_slider_kext_unslide(xpl_slider_kext_t slider, enum xpl_kext_segment segment, uintptr_t address);
void xpl_slider_kext_destroy(xpl_slider_kext_t* slider_p);


#endif /* xpl_slider_kext_h */
