#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sqlite3.h>

#define PORT 8080
#define BUFFER_SIZE 1024 * 1024
#define DB_NAME "server.db"

sqlite3 *g_db = NULL;
char *g_last_upload_data = NULL;
int g_last_upload_size = 0;

void send_response(int socket, int code, const char *status, const char *type, const char *body, int len) {
    char header[1024];
    if (len < 0) len = strlen(body);
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
             code, status, type, len);
    send(socket, header, strlen(header), 0);
    if (len > 0) send(socket, body, len, 0);
}

int main() {
    // Open database
    if (sqlite3_open(DB_NAME, &g_db) != SQLITE_OK) {
        fprintf(stderr, "Can't open database\n");
        return 1;
    }

    // Create simple storage table
    sqlite3_exec(g_db, "CREATE TABLE IF NOT EXISTS sync_data (id INTEGER PRIMARY KEY, data BLOB);", NULL, NULL, NULL);

    printf("Simple sync server listening on port %d\n", PORT);

    // Setup socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);

    while (1) {
        int client = accept(server_fd, NULL, NULL);
        char buffer[BUFFER_SIZE] = {0};
        int valread = read(client, buffer, BUFFER_SIZE - 1);

        if (valread > 0) {
            char method[16], path[2048];
            sscanf(buffer, "%15s %2047s", method, path);
            printf("Request: %s %s\n", method, path);

            if (strcmp(method, "GET") == 0 && strstr(path, "/upload")) {
                send_response(client, 200, "OK", "application/json",
                            "{\"url\":\"http://localhost:8080/data_upload\"}", -1);
            }
            else if (strcmp(method, "POST") == 0 && strstr(path, "/check")) {
                // Return empty (no changes)
                send_response(client, 204, "No Content", "text/plain", "", 0);
            }
            else if (strcmp(method, "PUT") == 0 && strstr(path, "/data_upload")) {
                int content_length = 0;
                char *cl = strstr(buffer, "Content-Length:");
                if (cl) content_length = atoi(cl + 15);

                printf("PUT with Content-Length: %d\n", content_length);

                if (content_length > 0) {
                    char *body_start = strstr(buffer, "\r\n\r\n");
                    if (body_start) {
                        body_start += 4;
                        int body_in_buffer = valread - (body_start - buffer);

                        if (g_last_upload_data) free(g_last_upload_data);
                        g_last_upload_data = malloc(content_length);
                        memcpy(g_last_upload_data, body_start, body_in_buffer);

                        int total = body_in_buffer;
                        int remaining = content_length - body_in_buffer;
                        while (remaining > 0) {
                            int r = read(client, g_last_upload_data + total, remaining);
                            if (r <= 0) break;
                            total += r;
                            remaining -= r;
                        }
                        g_last_upload_size = total;
                        printf("Received %d bytes\n", total);
                        send_response(client, 200, "OK", "text/plain", "OK", -1);
                    }
                }
            }
            else if (strcmp(method, "POST") == 0 && strstr(path, "/upload")) {
                if (g_last_upload_data && g_last_upload_size > 0) {
                    // Store in database
                    sqlite3_stmt *stmt;
                    sqlite3_prepare_v2(g_db, "INSERT INTO sync_data (data) VALUES (?)", -1, &stmt, NULL);
                    sqlite3_bind_blob(stmt, 1, g_last_upload_data, g_last_upload_size, SQLITE_STATIC);
                    sqlite3_step(stmt);
                    sqlite3_finalize(stmt);
                    printf("Stored %d bytes in database\n", g_last_upload_size);
                    send_response(client, 200, "OK", "text/plain", "Stored", -1);
                    free(g_last_upload_data);
                    g_last_upload_data = NULL;
                    g_last_upload_size = 0;
                } else {
                    send_response(client, 400, "Bad Request", "text/plain", "No data", -1);
                }
            }
            else {
                send_response(client, 404, "Not Found", "text/plain", "Not Found", -1);
            }
        }
        close(client);
    }

    return 0;
}
