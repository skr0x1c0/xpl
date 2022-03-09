//
//  tcc.h
//  playground
//
//  Created by sreejith on 3/8/22.
//

#ifndef xe_util_tcc_h
#define xe_util_tcc_h

#include <stdio.h>

int xe_util_tcc_modify_db(const char* db_path, const char* client, const char* service, int client_type, int auth_value, int auth_reason);
int xe_util_tcc_authorize(const char* db, const char* client, const char* service);

#endif /* xe_util_tcc_h */
