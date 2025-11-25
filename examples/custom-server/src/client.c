#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>

int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    for (int i = 0; i < argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <db> <sql>\n", argv[0]);
        return 1;
    }

    const char *db_name = argv[1];
    const char *sql = argv[2];

    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;

    rc = sqlite3_open(db_name, &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_enable_load_extension(db, 1);
    rc = sqlite3_load_extension(db, "./dist/cloudsync.dylib", NULL, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Can't load extension: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        return 1;
    }

    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}
