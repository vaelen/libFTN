/*
 * net.h - Network abstraction layer for libFTN
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef FTN_NET_H
#define FTN_NET_H

#include "ftn.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET ftn_socket_t;
#define FTN_INVALID_SOCKET INVALID_SOCKET
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
typedef int ftn_socket_t;
#define FTN_INVALID_SOCKET (-1)
#endif

/* Network connection structure */
typedef struct {
    ftn_socket_t socket;
    char* hostname;
    int port;
    int connected;
    int non_blocking;
    time_t connect_time;
    size_t bytes_sent;
    size_t bytes_received;
} ftn_net_connection_t;

/* Network server structure */
typedef struct {
    ftn_socket_t socket;
    int port;
    int listening;
    int max_connections;
    char* bind_address;
} ftn_net_server_t;

/* Network initialization and cleanup */
ftn_error_t ftn_net_init(void);
void ftn_net_cleanup(void);

/* Connection management */
ftn_net_connection_t* ftn_net_connect(const char* hostname, int port, int timeout_ms);
ftn_error_t ftn_net_disconnect(ftn_net_connection_t* conn);
void ftn_net_connection_free(ftn_net_connection_t* conn);

/* Data transmission */
ftn_error_t ftn_net_send(ftn_net_connection_t* conn, const void* data, size_t len, size_t* bytes_sent);
ftn_error_t ftn_net_recv(ftn_net_connection_t* conn, void* buffer, size_t len, size_t* bytes_received);
ftn_error_t ftn_net_send_all(ftn_net_connection_t* conn, const void* data, size_t len);
ftn_error_t ftn_net_recv_all(ftn_net_connection_t* conn, void* buffer, size_t len);

/* Non-blocking I/O */
ftn_error_t ftn_net_set_non_blocking(ftn_net_connection_t* conn, int non_blocking);
ftn_error_t ftn_net_select(ftn_net_connection_t** read_conns, size_t read_count,
                          ftn_net_connection_t** write_conns, size_t write_count,
                          int timeout_ms);

/* Server operations */
ftn_net_server_t* ftn_net_listen(int port, const char* bind_address, int max_connections);
ftn_net_connection_t* ftn_net_accept(ftn_net_server_t* server, int timeout_ms);
void ftn_net_server_free(ftn_net_server_t* server);

/* Socket options */
ftn_error_t ftn_net_set_keepalive(ftn_net_connection_t* conn, int enable);
ftn_error_t ftn_net_set_nodelay(ftn_net_connection_t* conn, int enable);
ftn_error_t ftn_net_set_timeout(ftn_net_connection_t* conn, int timeout_ms);

/* Utility functions */
ftn_error_t ftn_net_resolve_hostname(const char* hostname, char* ip_buffer, size_t buffer_size);
const char* ftn_net_get_error_string(ftn_error_t error);
int ftn_net_is_connected(ftn_net_connection_t* conn);

/* Error handling */
ftn_error_t ftn_net_get_last_error(void);
void ftn_net_clear_error(void);

#endif /* FTN_NET_H */