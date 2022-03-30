//
//  systemstatusd.h
//  playground
//
//  Created by sreejith on 3/9/22.
//

#ifndef xpl_privacy_h
#define xpl_privacy_h

#include <stdio.h>

typedef struct privacy_disable_session* privacy_disable_session_t;

privacy_disable_session_t privacy_disable_session_start(void);
void privacy_disable_session_stop(privacy_disable_session_t* session_p);

#endif /* xpl_privacy_h */
