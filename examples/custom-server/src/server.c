#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sqlite3.h>
#include <ctype.h>

// Include necessary headers from the extension
#include "src/cloudsync.h"
#include "src/cloudsync_private.h"
#include "src/network_private.h"
#include "src/dbutils.h"
#include "src/pk.h"
#include "src/lz4.h"

#define PORT 8080
#define BUFFER_SIZE 1024 * 1024 // 1MB buffer
#define DB_NAME "server.db"

// Global DB connection
sqlite3 *g_db = NULL;

// Helper to send HTTP response
void send_response(int client_socket, int status_code, const char *status_text, const char *content_type, const char *body, int body_len) {
    char header[1024];
    if (body_len < 0) body_len = strlen(body);

    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n",
             status_code, status_text, content_type, body_len);

    send(client_socket, header, strlen(header), 0);
    if (body_len > 0) {
        send(client_socket, body, body_len, 0);
    }
}

// Helper to parse query string
void parse_query(const char *query, int *db_version, int *seq) {
    // Expected format: .../check?apikey=... (but we are looking at the path components in the main loop)
    // Actually, the check URL is: /v1/cloudsync/{dbname}/{site_id}/{db_version}/{seq}/check
    // We will parse this in the main loop.
}

// Mock implementation of cloudsync_payload_apply_callback
bool mock_payload_apply_callback(void **xdata, cloudsync_pk_decode_bind_context *decoded_change, sqlite3 *db, cloudsync_context *data, int step, int rc) {
    return true;
}

// Handle /check request
// URL: /v1/cloudsync/{dbname}/{site_id}/{db_version}/{seq}/check
void handle_check(int client_socket, const char *path) {
    // Parse path to get db_version and seq
    // Format: /v1/cloudsync/server.db/SITEID/DBVER/SEQ/check

    int db_version = 0;
    int seq = 0;
    char site_id_str[64] = {0};

    // Quick and dirty parsing
    // Assuming fixed structure for simplicity
    char *p = strstr(path, "/v1/cloudsync/");
    if (!p) { send_response(client_socket, 400, "Bad Request", "text/plain", "Invalid path", -1); return; }

    // Skip prefix
    p += strlen("/v1/cloudsync/");

    // Skip dbname
    char *slash = strchr(p, '/');
    if (!slash) { send_response(client_socket, 400, "Bad Request", "text/plain", "Invalid path", -1); return; }
    p = slash + 1;

    // Get site_id
    slash = strchr(p, '/');
    if (!slash) { send_response(client_socket, 400, "Bad Request", "text/plain", "Invalid path", -1); return; }
    int site_id_len = slash - p;
    if (site_id_len < 63) {
        strncpy(site_id_str, p, site_id_len);
        site_id_str[site_id_len] = '\0';
    }
    p = slash + 1;

    // Get db_version
    slash = strchr(p, '/');
    if (!slash) { send_response(client_socket, 400, "Bad Request", "text/plain", "Invalid path", -1); return; }
    db_version = atoi(p);
    p = slash + 1;

    // Get seq
    slash = strchr(p, '/');
    if (!slash) { send_response(client_socket, 400, "Bad Request", "text/plain", "Invalid path", -1); return; }
    seq = atoi(p);

    printf("Check request: SiteID=%s, DBVer=%d, Seq=%d\n", site_id_str, db_version, seq);

    // Prepare context for cloudsync_payload_get
    // We need to mock sqlite3_context somewhat or just call the logic directly.
    // cloudsync_payload_get uses sqlite3_context_db_handle(context) to get the DB.
    // It also uses dbutils_settings_get_int_value to get requested version, but here we have them from URL.
    // The function `cloudsync_payload_get` reads CLOUDSYNC_KEY_SEND_DBVERSION from the DB settings.
    // We need to temporarily set these settings in the DB so `cloudsync_payload_get` picks them up.

    // In a real server, we wouldn't use the extension's client-side function `cloudsync_payload_get` directly like this
    // because it assumes it's running in the context of the "sender".
    // But here, the server IS the sender of updates to the client.
    // So we set the "SEND" keys to what the client requested (which is what the client HAS).
    // Wait, `cloudsync_payload_get` sends changes *after* the version specified in settings.
    // So if client has version X, we set SEND_DBVERSION to X, and it will fetch changes > X.

    char val_buf[32];
    sprintf(val_buf, "%d", db_version);
    dbutils_settings_set_key_value(g_db, NULL, CLOUDSYNC_KEY_SEND_DBVERSION, val_buf);

    sprintf(val_buf, "%d", seq);
    dbutils_settings_set_key_value(g_db, NULL, CLOUDSYNC_KEY_SEND_SEQ, val_buf);

    // Now call the logic. We can't easily call `cloudsync_payload_get` because it requires a `sqlite3_context`.
    // However, looking at `cloudsync_payload_get` source, it does:
    // 1. Get db handle
    // 2. Get db_version and seq from settings
    // 3. Run a query to get the blob

    // We can replicate step 3 directly.
    char *blob = NULL;
    int blob_size = 0;
    sqlite3_int64 new_db_version = 0;
    sqlite3_int64 new_seq = 0;

    char sql[2048];
    snprintf(sql, sizeof(sql),
             "WITH max_db_version AS (SELECT MAX(db_version) AS max_db_version FROM cloudsync_changes) "
             "SELECT cloudsync_payload_encode(tbl, pk, col_name, col_value, col_version, db_version, site_id, cl, seq), "
             "max_db_version AS max_db_version, "
             "MAX(IIF(db_version = max_db_version, seq, NULL)) "
             "FROM cloudsync_changes, max_db_version "
             "WHERE site_id != x'%s' " // Don't send back changes originating from the requestor?
             // Actually, standard logic is: send everything > version.
             // But usually we filter out changes that came from the same site_id to avoid echo.
             // The client sends its site_id in the URL.
             // The `cloudsync_payload_get` in the extension filters `WHERE site_id=cloudsync_siteid()`.
             // That is for *pushing* local changes.
             // Here we are *pulling* changes for a client.
             // We should send changes where site_id != client_site_id.
             // But wait, the `cloudsync_changes` table on the server contains changes from ALL clients.
             // We want to send all changes that are newer than what the client has, EXCEPT the ones the client sent itself.
             "AND (db_version > %d OR (db_version = %d AND seq > %d))",
             site_id_str, db_version, db_version, seq);

    // Note: The original `cloudsync_payload_get` uses `cloudsync_siteid()` which is the LOCAL site id.
    // On the server, we want to send changes from ANY site (except maybe the requester, but CRDT handles duplicates usually).
    // Let's try sending everything for now, or filter by site_id if we can convert the hex string to blob.
    // For simplicity, let's just send everything > version. The client should ignore what it already has or apply it idempotently.

    snprintf(sql, sizeof(sql),
             "WITH max_db_version AS (SELECT MAX(db_version) AS max_db_version FROM cloudsync_changes) "
             "SELECT cloudsync_payload_encode(tbl, pk, col_name, col_value, col_version, db_version, site_id, cl, seq), "
             "max_db_version AS max_db_version, "
             "MAX(IIF(db_version = max_db_version, seq, NULL)) "
             "FROM cloudsync_changes, max_db_version "
             "WHERE (db_version > %d OR (db_version = %d AND seq > %d))",
             db_version, db_version, seq);

    int rc = dbutils_blob_int_int_select(g_db, sql, &blob, &blob_size, &new_db_version, &new_seq);

    if (rc == SQLITE_OK && blob && blob_size > 0) {
        printf("Sending %d bytes of changes\n", blob_size);
        send_response(client_socket, 200, "OK", "application/octet-stream", blob, blob_size);
        sqlite3_free(blob);
    } else {
        printf("No new changes\n");
        send_response(client_socket, 204, "No Content", "text/plain", "", 0);
    }
}

// Handle /upload request (GET) - Returns the URL to POST data to
// URL: /v1/cloudsync/{dbname}/{site_id}/{upload}
void handle_upload_url(int client_socket, const char *path) {
    // We just return a URL that points back to this server's PUT endpoint.
    // We can construct it based on the incoming path.
    // For simplicity, let's assume we just append "/data" to the current path?
    // The client expects a JSON response: {"url": "..."}

    // The client calls `handle_upload_url` (GET) to get a presigned URL (usually S3).
    // Then it PUTs the data to that URL.
    // Then it calls `handle_upload_notify` (POST) to say "I'm done".

    // Let's construct a local URL.
    // Assuming we are localhost:8080
    // We need to preserve the path structure so we can parse site_id etc later if needed.
    // Let's just say the upload URL is http://localhost:8080/data_upload

    const char *json = "{\"url\":\"http://localhost:8080/data_upload\"}";
    send_response(client_socket, 200, "OK", "application/json", json, -1);
}

// Global buffer to hold the last uploaded data (very simplistic, single client)
char *g_last_upload_data = NULL;
int g_last_upload_size = 0;

// Handle data upload (PUT)
void handle_upload_data(int client_socket, int content_length) {
    if (g_last_upload_data) {
        free(g_last_upload_data);
        g_last_upload_data = NULL;
    }

    if (content_length <= 0) {
        send_response(client_socket, 400, "Bad Request", "text/plain", "Empty body", -1);
        return;
    }

    g_last_upload_data = malloc(content_length);
    if (!g_last_upload_data) {
        send_response(client_socket, 500, "Internal Server Error", "text/plain", "OOM", -1);
        return;
    }

    int total_read = 0;
    while (total_read < content_length) {
        int bytes_read = recv(client_socket, g_last_upload_data + total_read, content_length - total_read, 0);
        if (bytes_read <= 0) break;
        total_read += bytes_read;
    }

    g_last_upload_size = total_read;
    printf("Received upload data: %d bytes\n", g_last_upload_size);

    send_response(client_socket, 200, "OK", "text/plain", "Uploaded", -1);
}

// Handle upload notify (POST)
// URL: /v1/cloudsync/{dbname}/{site_id}/{upload}
void handle_upload_notify(int client_socket, const char *path) {
    // The client tells us it has finished uploading to the URL we gave it.
    // Now we should process the data in `g_last_upload_data`.

    if (!g_last_upload_data || g_last_upload_size == 0) {
        send_response(client_socket, 400, "Bad Request", "text/plain", "No data uploaded", -1);
        return;
    }

    // We need to apply this payload to our server database.
    // We can use `cloudsync_payload_apply`.
    // It requires a sqlite3_context. We need to mock it or call the internal logic.
    // `cloudsync_payload_apply` takes (sqlite3_context *context, const char *payload, int blen).
    // It uses context to get the db handle and report errors.

    // Since `cloudsync_payload_apply` is tied to the extension context, let's look at what it does.
    // It decodes header, decompresses, prepares INSERT statement, and loops over rows calling `pk_decode`.
    // We can try to invoke it by mocking the context.

    // Mock context

    // We can't easily mock sqlite3_context internals as they are opaque.

    // However, looking at `cloudsync.c`, `cloudsync_payload_apply` is not static.
    // But it calls `sqlite3_context_db_handle(context)`.
    // If we can't mock context, we might have to copy the logic of `cloudsync_payload_apply` or modify it to take `db` directly.
    // Modifying the source is risky/messy.

    // Alternative: The server is also an SQLite extension user?
    // No, the server is a standalone C program linking against the extension sources.
    // We can modify `cloudsync.c` to add a helper `cloudsync_payload_apply_db(sqlite3 *db, ...)`
    // OR we can just duplicate the logic here since we have access to all the private headers.

    // Let's duplicate the logic of `cloudsync_payload_apply` but adapted for `sqlite3 *db`.

    // ... (Logic from cloudsync_payload_apply) ...
    // Simplified version:

    const char *payload = g_last_upload_data;
    int blen = g_last_upload_size;

    cloudsync_payload_header header;
    memcpy(&header, payload, sizeof(cloudsync_payload_header));

    header.signature = ntohl(header.signature);
    header.expanded_size = ntohl(header.expanded_size);
    header.ncols = ntohs(header.ncols);
    header.nrows = ntohl(header.nrows);
    header.schema_hash = ntohll(header.schema_hash);

    if (header.signature != CLOUDSYNC_PAYLOAD_SIGNATURE) {
        send_response(client_socket, 400, "Bad Request", "text/plain", "Invalid signature", -1);
        return;
    }

    const char *buffer = payload + sizeof(cloudsync_payload_header);
    blen -= sizeof(cloudsync_payload_header);

    char *clone = NULL;
    if (header.expanded_size != 0) {
        clone = (char *)malloc(header.expanded_size);
        if (!clone) { send_response(client_socket, 500, "Error", "text/plain", "OOM", -1); return; }

        int rc = LZ4_decompress_safe(buffer, clone, blen, header.expanded_size);
        if (rc <= 0) {
            free(clone);
            send_response(client_socket, 400, "Bad Request", "text/plain", "Decompression failed", -1);
            return;
        }
        buffer = clone;
    }

    sqlite3_stmt *vm = NULL;
    const char *sql = "INSERT INTO cloudsync_changes(tbl, pk, col_name, col_value, col_version, db_version, site_id, cl, seq) VALUES (?,?,?,?,?,?,?,?,?);";
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &vm, NULL);
    if (rc != SQLITE_OK) {
        if (clone) free(clone);
        send_response(client_socket, 500, "Error", "text/plain", "SQL Error", -1);
        return;
    }

    cloudsync_pk_decode_bind_context decoded_context = {.vm = vm};
    size_t seek = 0;

    sqlite3_exec(g_db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    for (uint32_t i=0; i<header.nrows; ++i) {
        pk_decode((char *)buffer, blen, header.ncols, &seek, cloudsync_pk_decode_bind_callback, &decoded_context);

        // We need to handle the "apply callback" logic if we want to support conflict resolution on the server side?
        // For a simple sync server (relay), we just store the changes in `cloudsync_changes`.
        // The server acts as a "dumb" store that merges everything.
        // The `cloudsync_changes` table has a UNIQUE constraint or Primary Key?
        // In `cloudsync.c`, the table is defined.
        // Usually `cloudsync_changes` is a log.
        // Wait, `cloudsync_changes` is the table where we store changes to BE sent or THAT HAVE BEEN received.
        // If we just insert into it, we are good.

        sqlite3_step(vm);
        sqlite3_reset(vm);
        sqlite3_clear_bindings(vm);
    }

    sqlite3_exec(g_db, "COMMIT;", NULL, NULL, NULL);
    sqlite3_finalize(vm);
    if (clone) free(clone);

    // Clear buffer
    free(g_last_upload_data);
    g_last_upload_data = NULL;
    g_last_upload_size = 0;

    printf("Applied %d rows of changes\n", header.nrows);
    send_response(client_socket, 200, "OK", "text/plain", "Changes applied", -1);
}


int main() {
    // Initialize DB
    int rc = sqlite3_open(DB_NAME, &g_db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(g_db));
        return 1;
    }

    // Load the cloudsync extension so we have access to cloudsync_payload_encode, etc.
    sqlite3_enable_load_extension(g_db, 1);
    char *err_msg = NULL;
    rc = sqlite3_load_extension(g_db, "./dist/cloudsync.dylib", NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to load cloudsync extension: %s\n", err_msg ? err_msg : sqlite3_errmsg(g_db));
        sqlite3_free(err_msg);
        return 1;
    }
    printf("Cloudsync extension loaded successfully\n");

    // Enable extension logic (create tables)
    // We can call `cloudsync_init` logic or just manually create the table.
    // The server needs `cloudsync_changes` table.
    // Let's use the extension's init function if possible, or just run the SQL.
    // `cloudsync_init` initializes a specific table for sync.
    // We need the internal tables: `cloudsync_changes`, `cloudsync_settings`, etc.
    // These are created when the extension is loaded or when init is called.
    // Let's just create them manually to be sure.

    const char *init_sql =
    "CREATE TABLE IF NOT EXISTS cloudsync_changes ("
    "tbl TEXT, pk BLOB, col_name TEXT, col_value BLOB, col_version INTEGER, "
    "db_version INTEGER, site_id BLOB, cl INTEGER, seq INTEGER, "
    "PRIMARY KEY (tbl, pk, col_name, db_version, seq));"
    "CREATE TABLE IF NOT EXISTS cloudsync_settings (key TEXT PRIMARY KEY, value TEXT);"
    "CREATE TABLE IF NOT EXISTS cloudsync_schema_versions (hash INTEGER PRIMARY KEY, version INTEGER);";

    sqlite3_exec(g_db, init_sql, NULL, NULL, NULL);

    // Server setup
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }

        char buffer[BUFFER_SIZE] = {0};
        int valread = read(client_socket, buffer, BUFFER_SIZE - 1);
        if (valread > 0) {
            buffer[valread] = '\0'; // Ensure null termination

            // Parse HTTP method and path
            char method[16] = {0};
            char path[2048] = {0};
            char version[16] = {0};
            int parsed = sscanf(buffer, "%15s %2047s %15s", method, path, version);

            if (parsed < 2) {
                printf("Failed to parse request. First 100 bytes:\n%.100s\n", buffer);
                send_response(client_socket, 400, "Bad Request", "text/plain", "Malformed request", -1);
                close(client_socket);
                continue;
            }

            printf("Request: %s %s %s\n", method, path, version);

            // Check for Expect: 100-continue header (URLSession uses this)
            if (strstr(buffer, "Expect: 100-continue") || strstr(buffer, "Expect:100-continue")) {
                printf("Sending 100 Continue response\n");
                const char *continue_response = "HTTP/1.1 100 Continue\r\n\r\n";
                send(client_socket, continue_response, strlen(continue_response), 0);

                // Read the actual body after sending 100 Continue
                // This read will contain the body for PUT/POST requests.
                // The original 'buffer' now contains only headers.
                // We need to store the headers separately if we need them later,
                // but for Content-Length, we can re-parse the original buffer.
                // For simplicity, we'll assume the body is read into 'buffer' for PUT.
                // This means the 'buffer' will contain the body, and we'll need to find Content-Length from the *original* header part.
                // To handle this correctly, we should save the initial headers.
                // For this PoC, let's assume the initial 'buffer' still holds the headers for Content-Length parsing.
                // The subsequent 'read' will fill 'buffer' with the body.
                // This is a simplification and might not be robust for all cases.
                // A more robust solution would involve reading headers and body separately.
            }

            if (strcmp(method, "GET") == 0) {
                if (strstr(path, "/upload")) {
                    handle_upload_url(client_socket, path);
                } else {
                    send_response(client_socket, 404, "Not Found", "text/plain", "Not Found", -1);
                }
            } else if (strcmp(method, "POST") == 0) {
                if (strstr(path, "/check")) {
                    handle_check(client_socket, path);
                } else if (strstr(path, "/upload")) {
                    handle_upload_notify(client_socket, path);
                } else {
                    send_response(client_socket, 404, "Not Found", "text/plain", "Not Found", -1);
                }
            } else if (strcmp(method, "PUT") == 0) {
                if (strstr(path, "/data_upload")) {
                    // Parse Content-Length from the initial buffer (which contains headers)
                    int content_length = 0;
                    char *cl = strstr(buffer, "Content-Length:");
                    if (!cl) cl = strstr(buffer, "Content-Length :"); // Handle potential space
                    if (cl) {
                        content_length = atoi(cl + 15); // "Content-Length:" is 15 chars
                    }

                    printf("PUT request with Content-Length: %d\n", content_length);

                    // Find where the body starts
                    // If 100-continue was sent, 'buffer' might now contain the body.
                    // If not, 'body_start' will be found in the initial 'buffer'.
                    char *body_start = strstr(buffer, "\r\n\r\n");
                    if (body_start) {
                        body_start += 4;
                        int body_in_buffer = valread - (body_start - buffer);

                        if (content_length > 0) {
                            if (g_last_upload_data) free(g_last_upload_data);
                            g_last_upload_data = malloc(content_length);
                            if (!g_last_upload_data) {
                                send_response(client_socket, 500, "Internal Server Error", "text/plain", "OOM", -1);
                                close(client_socket);
                                continue;
                            }

                            memcpy(g_last_upload_data, body_start, body_in_buffer);
                            int remaining = content_length - body_in_buffer;
                            int total = body_in_buffer;

                            printf("Read %d bytes in initial buffer, %d remaining\n", body_in_buffer, remaining);

                            while (remaining > 0) {
                                int r = read(client_socket, g_last_upload_data + total, remaining);
                                if (r <= 0) {
                                    printf("Read failed or connection closed, got %d bytes total\n", total);
                                    break;
                                }
                                total += r;
                                remaining -= r;
                                printf("Read %d more bytes, %d remaining\n", r, remaining);
                            }
                            g_last_upload_size = total;
                            printf("Upload complete: %d bytes received\n", g_last_upload_size);
                            send_response(client_socket, 200, "OK", "text/plain", "Uploaded", -1);
                        } else {
                            send_response(client_socket, 411, "Length Required", "text/plain", "Content-Length required", -1);
                        }
                    } else {
                        // This case might happen if the initial read only contained headers and no body,
                        // and no 100-continue was sent, or if the body is truly missing.
                        // For PUT, a body is expected.
                        send_response(client_socket, 400, "Bad Request", "text/plain", "No body found", -1);
                    }
                } else {
                    send_response(client_socket, 404, "Not Found", "text/plain", "Not Found", -1);
                }
            } else {
                send_response(client_socket, 405, "Method Not Allowed", "text/plain", "Method Not Allowed", -1);
            }
        }

        close(client_socket);
    }

    return 0;
}
