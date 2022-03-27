//
//  core_entitlements.h
//  demo_entitlements
//
//  Created by sreejith on 3/4/22.
//

#ifndef core_entitlements_h
#define core_entitlements_h

#include <stdlib.h>

typedef struct CERuntime* CERuntimeRef;

extern long CESerializeCFDictionary(CERuntimeRef runtime, CFDictionaryRef dict, CFDataRef *data);

extern CERuntimeRef CECRuntime;
extern uint64_t kCEAPIMisuse, kCEAllocationFailed, kCEInvalidArgument, kCEMalformedEntitlements, kCENoError, kCEQueryCannotBeSatisfied;

#endif /* core_entitlements_h */
