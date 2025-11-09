// Copyright (C) 2018-2019, Cloudflare, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <ev.h>
#include <uthash.h>

#include <quiche.h>
#include <openssl/sha.h>

#define LOCAL_CONN_ID_LEN 16

#define MAX_DATAGRAM_SIZE 1350

#define MAX_TOKEN_LEN \
    sizeof("quiche") - 1 + \
    sizeof(struct sockaddr_storage) + \
    QUICHE_MAX_CONN_ID_LEN

struct connections {
    int sock;

    struct sockaddr *local_addr;
    socklen_t local_addr_len;

    struct conn_io *h;
};

struct conn_io {
    ev_timer timer;

    int sock;

    uint8_t cid[LOCAL_CONN_ID_LEN];

    quiche_conn *conn;

    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;

    UT_hash_handle hh;
};

// Structure to track pending file transfers
struct pending_transfer {
    uint64_t stream_id;
    uint8_t *data;          // File data buffer (owned by this structure)
    size_t offset;          // Current send offset
    size_t total_size;      // Total file size
    bool headers_sent;      // Whether HTTP headers were sent
    struct conn_io *conn_io; // Associated connection
    UT_hash_handle hh;      // Hash handle (keyed by stream_id)
};

static quiche_config *config = NULL;

static struct connections *conns = NULL;

// Global hash table for pending transfers (keyed by stream_id)
static struct pending_transfer *pending_transfers = NULL;

static void timeout_cb(EV_P_ ev_timer *w, int revents);
static void flush_egress(struct ev_loop *loop, struct conn_io *conn_io);

// Continue sending data from a pending transfer
// Returns: 1 if transfer complete, 0 if more data to send, -1 on error
static int continue_pending_transfer(struct pending_transfer *transfer) {
    if (!transfer || !transfer->conn_io || !transfer->data) {
        return -1;
    }

    uint64_t error_code = 0;
    const size_t CHUNK_SIZE = 8192;  // 8KB chunks

    while (transfer->offset < transfer->total_size) {
        size_t remaining = transfer->total_size - transfer->offset;
        size_t chunk_size = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        bool is_fin = (transfer->offset + chunk_size >= transfer->total_size);

        ssize_t sent = quiche_conn_stream_send(
            transfer->conn_io->conn,
            transfer->stream_id,
            transfer->data + transfer->offset,
            chunk_size,
            is_fin,
            &error_code
        );

        if (sent > 0) {
            transfer->offset += sent;
            fprintf(stderr, "Continued transfer on stream %" PRIu64 ": sent %zd bytes (offset: %zu/%zu)\n",
                    transfer->stream_id, sent, transfer->offset, transfer->total_size);

            // Check if transfer is complete
            if (transfer->offset >= transfer->total_size) {
                fprintf(stderr, "✓ Transfer complete on stream %" PRIu64 ": %zu/%zu bytes\n",
                        transfer->stream_id, transfer->offset, transfer->total_size);
                return 1;  // Complete
            }
        } else if (sent == QUICHE_ERR_DONE) {
            // Stream not writable yet, try again later
            fprintf(stderr, "Stream %" PRIu64 " not writable, will retry later (offset: %zu/%zu)\n",
                    transfer->stream_id, transfer->offset, transfer->total_size);
            return 0;  // Not complete, need to wait
        } else {
            fprintf(stderr, "Failed to send on stream %" PRIu64 ": %zd (error: %lu)\n",
                    transfer->stream_id, sent, error_code);
            return -1;  // Error
        }
    }

    return 1;  // Complete
}

static void debug_log(const char *line, void *argp) {
    fprintf(stderr, "%s\n", line);
}

// Get current time in milliseconds
static uint64_t get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

// Calculate SHA256 hash of data
static void calculate_sha256(const uint8_t *data, size_t len, char *output_hex) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data, len);
    SHA256_Final(hash, &sha256);

    // Convert to hex string
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output_hex + (i * 2), "%02x", hash[i]);
    }
    output_hex[SHA256_DIGEST_LENGTH * 2] = '\0';
}

// Parse HTTP request and extract URI
// Returns 1 on success, 0 on failure
static int parse_http_request(const char *request, size_t request_len, char *uri, size_t uri_max_len) {
    // Look for "GET /path HTTP/1.1"
    const char *method_end = strstr(request, " ");
    if (!method_end) {
        return 0;
    }

    // Skip method and space
    const char *uri_start = method_end + 1;
    const char *uri_end = strstr(uri_start, " ");
    if (!uri_end) {
        return 0;
    }

    size_t uri_len = uri_end - uri_start;
    if (uri_len >= uri_max_len) {
        return 0;
    }

    // Copy URI
    memcpy(uri, uri_start, uri_len);
    uri[uri_len] = '\0';

    return 1;
}

// Send HTTP response with file content
static void send_http_response(struct conn_io *conn_io, uint64_t stream_id,
                               const char *file_path) {
    uint64_t error_code;

    // Open and read file
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        // Send 404 Not Found
        const char *response =
            "HTTP/1.1 404 Not Found\r\n"
            "Server: HTTP-over-QUIC/1.0\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 13\r\n"
            "\r\n"
            "File not found";

        ssize_t sent = quiche_conn_stream_send(conn_io->conn, stream_id,
                                              (const uint8_t *)response,
                                              strlen(response), true, &error_code);

        fprintf(stderr, "Sent 404 response (%zd bytes) for %s\n", sent, file_path);
        return;
    }

    // Get file size
    struct stat st;
    if (fstat(fileno(file), &st) != 0) {
        fclose(file);
        fprintf(stderr, "Failed to get file stats\n");
        return;
    }
    size_t file_size = st.st_size;

    // Read file content
    uint8_t *file_data = malloc(file_size);
    if (!file_data) {
        fclose(file);
        fprintf(stderr, "Failed to allocate file buffer\n");
        return;
    }

    size_t read_size = fread(file_data, 1, file_size, file);
    fclose(file);

    if (read_size != file_size) {
        free(file_data);
        fprintf(stderr, "Failed to read file completely\n");
        return;
    }

    // Calculate SHA256
    char sha256_hex[SHA256_DIGEST_LENGTH * 2 + 1];
    calculate_sha256(file_data, file_size, sha256_hex);

    // Build HTTP response headers
    char headers[1024];
    int headers_len = snprintf(headers, sizeof(headers),
        "HTTP/1.1 200 OK\r\n"
        "Server: HTTP-over-QUIC/1.0\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: %zu\r\n"
        "X-Content-SHA256: %s\r\n"
        "\r\n",
        file_size, sha256_hex);

    fprintf(stderr, "Sending file %s (%zu bytes, SHA256: %s)\n",
            file_path, file_size, sha256_hex);

    // Send headers
    ssize_t sent = quiche_conn_stream_send(conn_io->conn, stream_id,
                                          (const uint8_t *)headers,
                                          headers_len, false, &error_code);

    if (sent < 0) {
        fprintf(stderr, "Failed to send headers: %zd\n", sent);
        free(file_data);
        return;
    }

    fprintf(stderr, "Sent %zd bytes of headers\n", sent);

    // Try to send file data in chunks
    const size_t CHUNK_SIZE = 8192;  // 8KB chunks
    size_t offset = 0;

    // Send as much as we can immediately
    while (offset < file_size) {
        size_t remaining = file_size - offset;
        size_t chunk_size = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        bool is_fin = (offset + chunk_size >= file_size);

        sent = quiche_conn_stream_send(conn_io->conn, stream_id,
                                      file_data + offset,
                                      chunk_size, is_fin, &error_code);

        if (sent > 0) {
            offset += sent;
            fprintf(stderr, "Sent chunk: %zd bytes (offset: %zu/%zu)\n",
                    sent, offset, file_size);

            // Check if partial send (sent less than we tried to send)
            if ((size_t)sent < chunk_size) {
                // Stream can't accept more data right now, create pending transfer
                fprintf(stderr, "Stream %" PRIu64 " partial send (%zd/%zu), creating pending transfer (offset: %zu/%zu)\n",
                        stream_id, sent, chunk_size, offset, file_size);

                struct pending_transfer *transfer = calloc(1, sizeof(*transfer));
                if (!transfer) {
                    fprintf(stderr, "Failed to allocate pending transfer\n");
                    free(file_data);
                    return;
                }

                transfer->stream_id = stream_id;
                transfer->data = file_data;  // Transfer ownership to pending_transfer
                transfer->offset = offset;
                transfer->total_size = file_size;
                transfer->headers_sent = true;
                transfer->conn_io = conn_io;

                // Add to hash table
                HASH_ADD(hh, pending_transfers, stream_id, sizeof(uint64_t), transfer);

                fprintf(stderr, "Created pending transfer for stream %" PRIu64 " (offset: %zu/%zu)\n",
                        stream_id, transfer->offset, transfer->total_size);

                // Don't free file_data - it's now owned by pending_transfer
                return;
            }
        } else if (sent == QUICHE_ERR_DONE) {
            // Stream not writable at all, create pending transfer for remaining data
            fprintf(stderr, "Stream %" PRIu64 " not writable, creating pending transfer (offset: %zu/%zu)\n",
                    stream_id, offset, file_size);

            struct pending_transfer *transfer = calloc(1, sizeof(*transfer));
            if (!transfer) {
                fprintf(stderr, "Failed to allocate pending transfer\n");
                free(file_data);
                return;
            }

            transfer->stream_id = stream_id;
            transfer->data = file_data;  // Transfer ownership to pending_transfer
            transfer->offset = offset;
            transfer->total_size = file_size;
            transfer->headers_sent = true;
            transfer->conn_io = conn_io;

            // Add to hash table
            HASH_ADD(hh, pending_transfers, stream_id, sizeof(uint64_t), transfer);

            fprintf(stderr, "Created pending transfer for stream %" PRIu64 " (offset: %zu/%zu)\n",
                    stream_id, transfer->offset, transfer->total_size);

            // Don't free file_data - it's now owned by pending_transfer
            return;
        } else {
            fprintf(stderr, "Failed to send data chunk: %zd (error: %lu)\n", sent, error_code);
            free(file_data);
            return;
        }
    }

    // All data sent successfully
    fprintf(stderr, "✓ File transfer complete: %zu/%zu bytes sent\n",
            offset, file_size);

    free(file_data);
}

static void flush_egress(struct ev_loop *loop, struct conn_io *conn_io) {
    static uint8_t out[MAX_DATAGRAM_SIZE];

    quiche_send_info send_info;

    while (1) {
        ssize_t written = quiche_conn_send(conn_io->conn, out, sizeof(out),
                                           &send_info);

        if (written == QUICHE_ERR_DONE) {
            fprintf(stderr, "done writing\n");
            break;
        }

        if (written < 0) {
            fprintf(stderr, "failed to create packet: %zd\n", written);
            return;
        }

        ssize_t sent = sendto(conn_io->sock, out, written, 0,
                              (struct sockaddr *) &send_info.to,
                              send_info.to_len);

        if (sent != written) {
            perror("failed to send");
            return;
        }

        fprintf(stderr, "sent %zd bytes\n", sent);
    }

    double t = quiche_conn_timeout_as_nanos(conn_io->conn) / 1e9f;
    conn_io->timer.repeat = t;
    ev_timer_again(loop, &conn_io->timer);
}

static void mint_token(const uint8_t *dcid, size_t dcid_len,
                       struct sockaddr_storage *addr, socklen_t addr_len,
                       uint8_t *token, size_t *token_len) {
    memcpy(token, "quiche", sizeof("quiche") - 1);
    memcpy(token + sizeof("quiche") - 1, addr, addr_len);
    memcpy(token + sizeof("quiche") - 1 + addr_len, dcid, dcid_len);

    *token_len = sizeof("quiche") - 1 + addr_len + dcid_len;
}

static bool validate_token(const uint8_t *token, size_t token_len,
                           struct sockaddr_storage *addr, socklen_t addr_len,
                           uint8_t *odcid, size_t *odcid_len) {
    if ((token_len < sizeof("quiche") - 1) ||
         memcmp(token, "quiche", sizeof("quiche") - 1)) {
        return false;
    }

    token += sizeof("quiche") - 1;
    token_len -= sizeof("quiche") - 1;

    if ((token_len < addr_len) || memcmp(token, addr, addr_len)) {
        return false;
    }

    token += addr_len;
    token_len -= addr_len;

    if (*odcid_len < token_len) {
        return false;
    }

    memcpy(odcid, token, token_len);
    *odcid_len = token_len;

    return true;
}

static uint8_t *gen_cid(uint8_t *cid, size_t cid_len) {
    int rng = open("/dev/urandom", O_RDONLY);
    if (rng < 0) {
        perror("failed to open /dev/urandom");
        return NULL;
    }

    ssize_t rand_len = read(rng, cid, cid_len);
    if (rand_len < 0) {
        perror("failed to create connection ID");
        return NULL;
    }

    return cid;
}

static struct conn_io *create_conn(uint8_t *scid, size_t scid_len,
                                   uint8_t *odcid, size_t odcid_len,
                                   struct sockaddr *local_addr,
                                   socklen_t local_addr_len,
                                   struct sockaddr_storage *peer_addr,
                                   socklen_t peer_addr_len)
{
    struct conn_io *conn_io = calloc(1, sizeof(*conn_io));
    if (conn_io == NULL) {
        fprintf(stderr, "failed to allocate connection IO\n");
        return NULL;
    }

    if (scid_len != LOCAL_CONN_ID_LEN) {
        fprintf(stderr, "failed, scid length too short\n");
    }

    memcpy(conn_io->cid, scid, LOCAL_CONN_ID_LEN);

    quiche_conn *conn = quiche_accept(conn_io->cid, LOCAL_CONN_ID_LEN,
                                      odcid, odcid_len,
                                      local_addr,
                                      local_addr_len,
                                      (struct sockaddr *) peer_addr,
                                      peer_addr_len,
                                      config);

    if (conn == NULL) {
        fprintf(stderr, "failed to create connection\n");
        return NULL;
    }

    conn_io->sock = conns->sock;
    conn_io->conn = conn;

    memcpy(&conn_io->peer_addr, peer_addr, peer_addr_len);
    conn_io->peer_addr_len = peer_addr_len;

    ev_init(&conn_io->timer, timeout_cb);
    conn_io->timer.data = conn_io;

    HASH_ADD(hh, conns->h, cid, LOCAL_CONN_ID_LEN, conn_io);

    fprintf(stderr, "new connection\n");

    return conn_io;
}

static void recv_cb(EV_P_ ev_io *w, int revents) {
    struct conn_io *tmp, *conn_io = NULL;

    static uint8_t buf[65535];
    static uint8_t out[MAX_DATAGRAM_SIZE];

    while (1) {
        struct sockaddr_storage peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);
        memset(&peer_addr, 0, peer_addr_len);

        ssize_t read = recvfrom(conns->sock, buf, sizeof(buf), 0,
                                (struct sockaddr *) &peer_addr,
                                &peer_addr_len);

        if (read < 0) {
            if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
                fprintf(stderr, "recv would block\n");
                break;
            }

            perror("failed to read");
            return;
        }

        uint8_t type;
        uint32_t version;

        uint8_t scid[QUICHE_MAX_CONN_ID_LEN];
        size_t scid_len = sizeof(scid);

        uint8_t dcid[QUICHE_MAX_CONN_ID_LEN];
        size_t dcid_len = sizeof(dcid);

        uint8_t odcid[QUICHE_MAX_CONN_ID_LEN];
        size_t odcid_len = sizeof(odcid);

        uint8_t token[MAX_TOKEN_LEN];
        size_t token_len = sizeof(token);

        int rc = quiche_header_info(buf, read, LOCAL_CONN_ID_LEN, &version,
                                    &type, scid, &scid_len, dcid, &dcid_len,
                                    token, &token_len);
        if (rc < 0) {
            fprintf(stderr, "failed to parse header: %d\n", rc);
            continue;
        }

        HASH_FIND(hh, conns->h, dcid, dcid_len, conn_io);

        if (conn_io == NULL) {
            if (!quiche_version_is_supported(version)) {
                fprintf(stderr, "version negotiation\n");

                ssize_t written = quiche_negotiate_version(scid, scid_len,
                                                           dcid, dcid_len,
                                                           out, sizeof(out));

                if (written < 0) {
                    fprintf(stderr, "failed to create vneg packet: %zd\n",
                            written);
                    continue;
                }

                ssize_t sent = sendto(conns->sock, out, written, 0,
                                      (struct sockaddr *) &peer_addr,
                                      peer_addr_len);
                if (sent != written) {
                    perror("failed to send");
                    continue;
                }

                fprintf(stderr, "sent %zd bytes\n", sent);
                continue;
            }

            if (token_len == 0) {
                fprintf(stderr, "stateless retry\n");

                mint_token(dcid, dcid_len, &peer_addr, peer_addr_len,
                           token, &token_len);

                uint8_t new_cid[LOCAL_CONN_ID_LEN];

                if (gen_cid(new_cid, LOCAL_CONN_ID_LEN) == NULL) {
                    continue;
                }

                ssize_t written = quiche_retry(scid, scid_len,
                                               dcid, dcid_len,
                                               new_cid, LOCAL_CONN_ID_LEN,
                                               token, token_len,
                                               version, out, sizeof(out));

                if (written < 0) {
                    fprintf(stderr, "failed to create retry packet: %zd\n",
                            written);
                    continue;
                }

                ssize_t sent = sendto(conns->sock, out, written, 0,
                                      (struct sockaddr *) &peer_addr,
                                      peer_addr_len);
                if (sent != written) {
                    perror("failed to send");
                    continue;
                }

                fprintf(stderr, "sent %zd bytes\n", sent);
                continue;
            }


            if (!validate_token(token, token_len, &peer_addr, peer_addr_len,
                               odcid, &odcid_len)) {
                fprintf(stderr, "invalid address validation token\n");
                continue;
            }

            conn_io = create_conn(dcid, dcid_len, odcid, odcid_len,
                                  conns->local_addr, conns->local_addr_len,
                                  &peer_addr, peer_addr_len);

            if (conn_io == NULL) {
                continue;
            }
        }

        quiche_recv_info recv_info = {
            (struct sockaddr *)&peer_addr,
            peer_addr_len,

            conns->local_addr,
            conns->local_addr_len,
        };

        ssize_t done = quiche_conn_recv(conn_io->conn, buf, read, &recv_info);

        if (done < 0) {
            fprintf(stderr, "failed to process packet: %zd\n", done);
            continue;
        }

        fprintf(stderr, "recv %zd bytes\n", done);

        if (quiche_conn_is_established(conn_io->conn)) {
            uint64_t s = 0;

            quiche_stream_iter *readable = quiche_conn_readable(conn_io->conn);

            while (quiche_stream_iter_next(readable, &s)) {
                fprintf(stderr, "stream %" PRIu64 " is readable\n", s);

                bool fin = false;
                uint64_t error_code;

                // Buffer to accumulate HTTP request
                static uint8_t request_buf[8192];
                size_t request_len = 0;

                // Read all available data from stream
                while (1) {
                    ssize_t recv_len = quiche_conn_stream_recv(conn_io->conn, s,
                                                               buf, sizeof(buf),
                                                               &fin, &error_code);
                    if (recv_len < 0) {
                        break;
                    }

                    if (recv_len > 0) {
                        // Accumulate request data
                        if (request_len + recv_len < sizeof(request_buf)) {
                            memcpy(request_buf + request_len, buf, recv_len);
                            request_len += recv_len;
                        }
                        fprintf(stderr, "✓ Received %zd bytes on stream %" PRIu64 "\n", recv_len, s);
                    }

                    if (fin) {
                        fprintf(stderr, "✓ Client stream %" PRIu64 " finished. Total received: %zu bytes\n",
                                s, request_len);
                        break;
                    }

                    // No more data available right now
                    if (recv_len == 0) {
                        break;
                    }
                }

                // Process HTTP request
                if (request_len > 0) {
                    request_buf[request_len] = '\0';  // Null-terminate for string operations

                    fprintf(stderr, "HTTP Request:\n%s\n", request_buf);

                    // Parse HTTP request to extract URI
                    char uri[512];
                    if (parse_http_request((const char *)request_buf, request_len, uri, sizeof(uri))) {
                        fprintf(stderr, "Parsed URI: %s\n", uri);

                        // Construct file path: data/ + URI
                        char file_path[1024];
                        snprintf(file_path, sizeof(file_path), "data%s", uri);

                        fprintf(stderr, "File path: %s\n", file_path);

                        // Send HTTP response with file content
                        send_http_response(conn_io, s, file_path);
                    } else {
                        fprintf(stderr, "Failed to parse HTTP request\n");

                        // Send 400 Bad Request
                        const char *bad_request =
                            "HTTP/1.1 400 Bad Request\r\n"
                            "Server: HTTP-over-QUIC/1.0\r\n"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: 11\r\n"
                            "\r\n"
                            "Bad Request";

                        quiche_conn_stream_send(conn_io->conn, s,
                                              (const uint8_t *)bad_request,
                                              strlen(bad_request), true, &error_code);
                    }
                }
            }

            quiche_stream_iter_free(readable);

            // Check for writable streams and continue pending transfers
            uint64_t s_writable = 0;
            quiche_stream_iter *writable = quiche_conn_writable(conn_io->conn);

            while (quiche_stream_iter_next(writable, &s_writable)) {
                fprintf(stderr, "stream %" PRIu64 " is writable\n", s_writable);

                // Look up pending transfer for this stream
                struct pending_transfer *transfer = NULL;
                HASH_FIND(hh, pending_transfers, &s_writable, sizeof(uint64_t), transfer);

                if (transfer) {
                    fprintf(stderr, "Found pending transfer for stream %" PRIu64 ", continuing...\n",
                            s_writable);

                    int result = continue_pending_transfer(transfer);

                    if (result == 1) {
                        // Transfer complete, clean up
                        fprintf(stderr, "✓ Pending transfer complete for stream %" PRIu64 ", cleaning up\n",
                                s_writable);

                        HASH_DELETE(hh, pending_transfers, transfer);
                        free(transfer->data);
                        free(transfer);
                    } else if (result < 0) {
                        // Error occurred, clean up
                        fprintf(stderr, "✗ Error in pending transfer for stream %" PRIu64 ", cleaning up\n",
                                s_writable);

                        HASH_DELETE(hh, pending_transfers, transfer);
                        free(transfer->data);
                        free(transfer);
                    }
                    // result == 0: more data to send, will retry on next writable event
                }
            }

            quiche_stream_iter_free(writable);
        }
    }

    HASH_ITER(hh, conns->h, conn_io, tmp) {
        flush_egress(loop, conn_io);

        if (quiche_conn_is_closed(conn_io->conn)) {
            quiche_stats stats;
            quiche_path_stats path_stats;

            quiche_conn_stats(conn_io->conn, &stats);
            quiche_conn_path_stats(conn_io->conn, 0, &path_stats);

            fprintf(stderr, "connection closed, recv=%zu sent=%zu lost=%zu rtt=%" PRIu64 "ns cwnd=%zu\n",
                    stats.recv, stats.sent, stats.lost, path_stats.rtt, path_stats.cwnd);

            HASH_DELETE(hh, conns->h, conn_io);

            ev_timer_stop(loop, &conn_io->timer);
            quiche_conn_free(conn_io->conn);
            free(conn_io);
        }
    }
}

static void timeout_cb(EV_P_ ev_timer *w, int revents) {
    struct conn_io *conn_io = w->data;
    quiche_conn_on_timeout(conn_io->conn);

    fprintf(stderr, "timeout\n");

    flush_egress(loop, conn_io);

    if (quiche_conn_is_closed(conn_io->conn)) {
        quiche_stats stats;
        quiche_path_stats path_stats;

        quiche_conn_stats(conn_io->conn, &stats);
        quiche_conn_path_stats(conn_io->conn, 0, &path_stats);

        fprintf(stderr, "connection closed, recv=%zu sent=%zu lost=%zu rtt=%" PRIu64 "ns cwnd=%zu\n",
                stats.recv, stats.sent, stats.lost, path_stats.rtt, path_stats.cwnd);

        HASH_DELETE(hh, conns->h, conn_io);

        ev_timer_stop(loop, &conn_io->timer);
        quiche_conn_free(conn_io->conn);
        free(conn_io);

        return;
    }
}

int main(int argc, char *argv[]) {
    const char *host = argv[1];
    const char *port = argv[2];

    const struct addrinfo hints = {
        .ai_family = PF_UNSPEC,
        .ai_socktype = SOCK_DGRAM,
        .ai_protocol = IPPROTO_UDP
    };

    quiche_enable_debug_logging(debug_log, NULL);

    struct addrinfo *local;
    if (getaddrinfo(host, port, &hints, &local) != 0) {
        perror("failed to resolve host");
        return -1;
    }

    int sock = socket(local->ai_family, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("failed to create socket");
        return -1;
    }

    if (fcntl(sock, F_SETFL, O_NONBLOCK) != 0) {
        perror("failed to make socket non-blocking");
        return -1;
    }

    if (bind(sock, local->ai_addr, local->ai_addrlen) < 0) {
        perror("failed to connect socket");
        return -1;
    }

    config = quiche_config_new(QUICHE_PROTOCOL_VERSION);
    if (config == NULL) {
        fprintf(stderr, "failed to create config\n");
        return -1;
    }

    quiche_config_load_cert_chain_from_pem_file(config, "./cert.crt");
    quiche_config_load_priv_key_from_pem_file(config, "./cert.key");

    quiche_config_set_application_protos(config,
        (uint8_t *) "\x0ahq-interop\x05hq-29\x05hq-28\x05hq-27\x08http/0.9", 38);

    // Increased timeouts and limits for high throughput data transfer
    quiche_config_set_max_idle_timeout(config, 30000);  // 30 seconds
    quiche_config_set_max_recv_udp_payload_size(config, MAX_DATAGRAM_SIZE);
    quiche_config_set_max_send_udp_payload_size(config, MAX_DATAGRAM_SIZE);
    quiche_config_set_initial_max_data(config, 100000000);  // 100MB
    quiche_config_set_initial_max_stream_data_bidi_local(config, 50000000);   // 50MB
    quiche_config_set_initial_max_stream_data_bidi_remote(config, 50000000);  // 50MB
    quiche_config_set_initial_max_streams_bidi(config, 100);
    quiche_config_set_cc_algorithm(config, QUICHE_CC_RENO);

    struct connections c;
    c.sock = sock;
    c.h = NULL;
    c.local_addr = local->ai_addr;
    c.local_addr_len = local->ai_addrlen;

    conns = &c;

    ev_io watcher;

    struct ev_loop *loop = ev_default_loop(0);

    ev_io_init(&watcher, recv_cb, sock, EV_READ);
    ev_io_start(loop, &watcher);
    watcher.data = &c;

    ev_loop(loop, 0);

    freeaddrinfo(local);

    quiche_config_free(config);

    return 0;
}
