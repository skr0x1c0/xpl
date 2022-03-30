
#include <sqlite3.h>

#include "util/tcc.h"
#include "util/assert.h"
#include "util/log.h"


int xpl_tcc_remove_entry(sqlite3* db, const char* client, const char* service) {
    const char delete_statement[] = "DELETE FROM access WHERE client = ? AND service = ?;";
    
    sqlite3_stmt* stmt;
    int error = sqlite3_prepare_v2(db, delete_statement, sizeof(delete_statement), &stmt, NULL);
    xpl_assert_cond(error, ==, SQLITE_OK);
    error = sqlite3_bind_text(stmt, 1, client, -1, NULL);
    xpl_assert_cond(error, ==, SQLITE_OK);
    error = sqlite3_bind_text(stmt, 2, service, -1, NULL);
    xpl_assert_cond(error, ==, SQLITE_OK);
    
    error = sqlite3_step(stmt);
    if (error != SQLITE_DONE) {
        xpl_assert(error != 0);
        sqlite3_finalize(stmt);
        return error;
    }
    
    return sqlite3_finalize(stmt);;
}


int xpl_tcc_add_entry(sqlite3* db, const char* client, const char* service, int client_type, int auth_value, int auth_reason) {
    sqlite3_stmt* stmt;
    const char insert_statement[] = "INSERT INTO access (client, service, client_type, auth_value, auth_reason, auth_version, indirect_object_identifier_type, indirect_object_identifier, flags) VALUES (?, ?, ?, ?, ?, 1, 0, \"UNUSED\", 0);";
    int error = sqlite3_prepare_v2(db, insert_statement, sizeof(insert_statement), &stmt, NULL);
    xpl_assert_cond(error, ==, SQLITE_OK);
    
    error = sqlite3_bind_text(stmt, 1, client, -1, NULL);
    xpl_assert_cond(error, ==, SQLITE_OK);
    error = sqlite3_bind_text(stmt, 2, service, -1, NULL);
    xpl_assert_cond(error, ==, SQLITE_OK);
    error = sqlite3_bind_int(stmt, 3, client_type);
    xpl_assert_cond(error, ==, SQLITE_OK);
    error = sqlite3_bind_int(stmt, 4, auth_value);
    xpl_assert_cond(error, ==, SQLITE_OK);
    error = sqlite3_bind_int(stmt, 5, auth_reason);
    xpl_assert_cond(error, ==, SQLITE_OK);
    
    error = sqlite3_step(stmt);
    if (error != SQLITE_DONE) {
        xpl_assert(error != 0);
        sqlite3_finalize(stmt);
        return error;
    }
    
    
    return sqlite3_finalize(stmt);
}


int xpl_tcc_modify_db(const char* db_path, const char* client, const char* service, int client_type, int auth_value, int auth_reason) {
    sqlite3* db;
    int error = sqlite3_open(db_path, &db);
    if (error) {
        return error;
    }
    
    error = xpl_tcc_remove_entry(db, client, service);
    if (error) {
        xpl_log_error("removing service: %s, client: %s from db: %s failed, err: %d", service, client, db_path, error);
        goto exit;
    }
    
    error = xpl_tcc_add_entry(db, client, service, client_type, auth_value, auth_reason);
    if (error) {
        xpl_log_error("adding service: %s, client: %s to db: %s failed, err: %d", service, client, db_path, error);
        goto exit;
    }
    
exit:
    
    if (sqlite3_close(db) != SQLITE_OK) {
        xpl_log_error("cannot close sqlite db: %s", db_path);
    }
    
    return error;
}


int xpl_tcc_authorize(const char* db, const char* client, const char* service) {
    return xpl_tcc_modify_db(db, client, service, 0, 2, 4);
}
