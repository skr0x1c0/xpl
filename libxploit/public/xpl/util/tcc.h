//
//  tcc.h
//  playground
//
//  Created by sreejith on 3/8/22.
//

#ifndef xpl_tcc_h
#define xpl_tcc_h

#include <stdio.h>

/// Removes any existing entry in access table of provided TCC database with same client and
/// service value and inserts a new entry with provided values
/// @param db_path path to TCC.db
/// @param client value of client in access table
/// @param service value of service in access table
/// @param client_type value of client_type to be set in access table of TCC.db
/// @param auth_value value of auth_value to be set in access table of TCC.db
/// @param auth_reason value of auth_reason to be set in access table of TCC.db
int xpl_tcc_modify_db(const char* db_path, const char* client, const char* service, int client_type, int auth_value, int auth_reason);

/// Modify access table in provided TCC database to authorize provided service for provided client
/// @param db path to TCC database
/// @param client client for which `service` must be authorized
/// @param service TCC service
int xpl_tcc_authorize(const char* db, const char* client, const char* service);

#endif /* xpl_tcc_h */
