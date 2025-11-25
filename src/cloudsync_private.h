//
//  cloudsync_private.h
//  cloudsync
//
//  Created by Marco Bambini on 30/05/25.
//

#ifndef __CLOUDSYNC_PRIVATE__
#define __CLOUDSYNC_PRIVATE__

#include <stdbool.h>
#include <stdint.h>
#ifndef SQLITE_CORE
#include "sqlite3ext.h"
#else
#include "sqlite3.h"
#endif


#define CLOUDSYNC_TOMBSTONE_VALUE               "__[RIP]__"
#define CLOUDSYNC_RLS_RESTRICTED_VALUE          "__[RLS]__"
#define CLOUDSYNC_DISABLE_ROWIDONLY_TABLES      1
#define CLOUDSYNC_PAYLOAD_SIGNATURE             'CLSY'

typedef enum {
    CLOUDSYNC_PAYLOAD_APPLY_WILL_APPLY   = 1,
    CLOUDSYNC_PAYLOAD_APPLY_DID_APPLY    = 2,
    CLOUDSYNC_PAYLOAD_APPLY_CLEANUP      = 3
} CLOUDSYNC_PAYLOAD_APPLY_STEPS;

typedef struct cloudsync_context cloudsync_context;
typedef struct cloudsync_pk_decode_bind_context cloudsync_pk_decode_bind_context;
struct cloudsync_pk_decode_bind_context {
    sqlite3_stmt    *vm;
    char            *tbl;
    int64_t         tbl_len;
    const void      *pk;
    int64_t         pk_len;
    char            *col_name;
    int64_t         col_name_len;
    int64_t         col_version;
    int64_t         db_version;
    const void      *site_id;
    int64_t         site_id_len;
    int64_t         cl;
    int64_t         seq;
};

int cloudsync_merge_insert (sqlite3_vtab *vtab, int argc, sqlite3_value **argv, sqlite3_int64 *rowid);
void cloudsync_sync_key (cloudsync_context *data, const char *key, const char *value);

// used by network layer
const char *cloudsync_context_init (sqlite3 *db, cloudsync_context *data, sqlite3_context *context);
void *cloudsync_get_auxdata (sqlite3_context *context);
void cloudsync_set_auxdata (sqlite3_context *context, void *xdata);
int cloudsync_pk_decode_bind_callback (void *xdata, int index, int type, int64_t ival, double dval, char *pval);
int cloudsync_payload_apply (sqlite3_context *context, const char *payload, int blen);
int cloudsync_payload_get (sqlite3_context *context, char **blob, int *blob_size, int *db_version, int *seq, sqlite3_int64 *new_db_version, sqlite3_int64 *new_seq);

// used by core
typedef bool (*cloudsync_payload_apply_callback_t)(void **xdata, cloudsync_pk_decode_bind_context *decoded_change, sqlite3 *db, cloudsync_context *data, int step, int rc);
void cloudsync_set_payload_apply_callback(sqlite3 *db, cloudsync_payload_apply_callback_t callback);

bool cloudsync_config_exists (sqlite3 *db);
sqlite3_stmt *cloudsync_colvalue_stmt (sqlite3 *db, cloudsync_context *data, const char *tbl_name, bool *persistent);
char *cloudsync_pk_context_tbl (cloudsync_pk_decode_bind_context *ctx, int64_t *tbl_len);
void *cloudsync_pk_context_pk (cloudsync_pk_decode_bind_context *ctx, int64_t *pk_len);
char *cloudsync_pk_context_colname (cloudsync_pk_decode_bind_context *ctx, int64_t *colname_len);
int64_t cloudsync_pk_context_cl (cloudsync_pk_decode_bind_context *ctx);
int64_t cloudsync_pk_context_dbversion (cloudsync_pk_decode_bind_context *ctx);


#ifdef _MSC_VER
    #pragma pack(push, 1) // For MSVC: pack struct with 1-byte alignment
    #define PACKED
#else
    #define PACKED __attribute__((__packed__))
#endif

typedef struct PACKED {
    uint32_t    signature;         // 'CLSY'
    uint8_t     version;           // protocol version
    uint8_t     libversion[3];     // major.minor.patch
    uint32_t    expanded_size;
    uint16_t    ncols;
    uint32_t    nrows;
    uint64_t    schema_hash;
    uint8_t     unused[6];        // padding to ensure the struct is exactly 32 bytes
} cloudsync_payload_header;

typedef struct {
    sqlite3_value   *table_name;
    sqlite3_value   **new_values;
    sqlite3_value   **old_values;
    int             count;
    int             capacity;
} cloudsync_update_payload;

#ifdef _MSC_VER
    #pragma pack(pop)
#endif

#endif
