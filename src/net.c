/*
 * net.c - Network abstraction layer implementation
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "ftn.h"
#include "ftn/net.h"

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

static int ftn_net_initialized = 0;

/* Network initialization and cleanup */
ftn_error_t ftn_net_init(void) {
    if (ftn_net_initialized) {
        return FTN_OK;
    }

#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        return FTN_ERROR_NETWORK;
    }
#endif

    ftn_net_initialized = 1;
    return FTN_OK;
}

void ftn_net_cleanup(void) {
    if (!ftn_net_initialized) {
        return;
    }

#ifdef _WIN32
    WSACleanup();
#endif

    ftn_net_initialized = 0;
}

/* Helper function to close socket */
static void ftn_net_close_socket(ftn_socket_t socket) {
    if (socket != FTN_INVALID_SOCKET) {
#ifdef _WIN32
        closesocket(socket);
#else
        close(socket);
#endif
    }
}

/* Helper function to get last socket error */
static int ftn_net_get_socket_error(void) {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

/* Helper function to set socket non-blocking */
static ftn_error_t ftn_net_set_socket_non_blocking(ftn_socket_t socket, int non_blocking) {
#ifdef _WIN32
    unsigned long mode = non_blocking ? 1 : 0;
    if (ioctlsocket(socket, FIONBIO, &mode) != 0) {
        return FTN_ERROR_NETWORK;
    }
#else
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
        return FTN_ERROR_NETWORK;
    }

    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(socket, F_SETFL, flags) == -1) {
        return FTN_ERROR_NETWORK;
    }
#endif

    return FTN_OK;
}

/* Connection management */
ftn_net_connection_t* ftn_net_connect(const char* hostname, int port, int timeout_ms) {
    ftn_net_connection_t* conn;
    struct sockaddr_in addr;
    struct hostent* host;
    ftn_socket_t sock;
    int result;

    if (!hostname || port <= 0 || port > 65535) {
        return NULL;
    }

    if (!ftn_net_initialized) {
        if (ftn_net_init() != FTN_OK) {
            return NULL;
        }
    }

    /* Allocate connection structure */
    conn = malloc(sizeof(ftn_net_connection_t));
    if (!conn) {
        return NULL;
    }

    memset(conn, 0, sizeof(ftn_net_connection_t));
    conn->socket = FTN_INVALID_SOCKET;
    conn->port = port;
    conn->hostname = malloc(strlen(hostname) + 1);
    if (!conn->hostname) {
        free(conn);
        return NULL;
    }
    strcpy(conn->hostname, hostname);

    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == FTN_INVALID_SOCKET) {
        ftn_net_connection_free(conn);
        return NULL;
    }

    /* Resolve hostname */
    host = gethostbyname(hostname);
    if (!host) {
        ftn_net_close_socket(sock);
        ftn_net_connection_free(conn);
        return NULL;
    }

    /* Setup address structure */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);

    /* Set socket non-blocking for timeout */
    if (timeout_ms > 0) {
        ftn_net_set_socket_non_blocking(sock, 1);
    }

    /* Connect */
    result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (result == -1) {
        int error = ftn_net_get_socket_error();
#ifdef _WIN32
        if (error != WSAEWOULDBLOCK) {
#else
        if (error != EINPROGRESS) {
#endif
            ftn_net_close_socket(sock);
            ftn_net_connection_free(conn);
            return NULL;
        }

        /* Wait for connection with timeout */
        if (timeout_ms > 0) {
            fd_set write_fds;
            struct timeval tv;

            FD_ZERO(&write_fds);
            FD_SET(sock, &write_fds);

            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

            result = select(sock + 1, NULL, &write_fds, NULL, &tv);
            if (result <= 0) {
                ftn_net_close_socket(sock);
                ftn_net_connection_free(conn);
                return NULL;
            }

            /* Check if connection succeeded */
            {
                int error_code;
                socklen_t len = sizeof(error_code);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error_code, &len) < 0 || error_code != 0) {
                    ftn_net_close_socket(sock);
                    ftn_net_connection_free(conn);
                    return NULL;
                }
            }
        }

        /* Reset to blocking mode */
        ftn_net_set_socket_non_blocking(sock, 0);
    }

    conn->socket = sock;
    conn->connected = 1;
    conn->connect_time = time(NULL);

    return conn;
}

ftn_error_t ftn_net_disconnect(ftn_net_connection_t* conn) {
    if (!conn) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    if (conn->socket != FTN_INVALID_SOCKET) {
        ftn_net_close_socket(conn->socket);
        conn->socket = FTN_INVALID_SOCKET;
    }

    conn->connected = 0;
    return FTN_OK;
}

void ftn_net_connection_free(ftn_net_connection_t* conn) {
    if (!conn) {
        return;
    }

    ftn_net_disconnect(conn);

    if (conn->hostname) {
        free(conn->hostname);
    }

    free(conn);
}

/* Data transmission */
ftn_error_t ftn_net_send(ftn_net_connection_t* conn, const void* data, size_t len, size_t* bytes_sent) {
    ssize_t result;

    if (!conn || !data || !bytes_sent) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    if (!conn->connected || conn->socket == FTN_INVALID_SOCKET) {
        return FTN_ERROR_NOT_CONNECTED;
    }

    result = send(conn->socket, (const char*)data, len, 0);
    if (result == -1) {
        int error = ftn_net_get_socket_error();
#ifdef _WIN32
        if (error == WSAEWOULDBLOCK) {
#else
        if (error == EAGAIN || error == EWOULDBLOCK) {
#endif
            *bytes_sent = 0;
            return FTN_ERROR_WOULD_BLOCK;
        }
        return FTN_ERROR_NETWORK;
    }

    *bytes_sent = (size_t)result;
    conn->bytes_sent += *bytes_sent;
    return FTN_OK;
}

ftn_error_t ftn_net_recv(ftn_net_connection_t* conn, void* buffer, size_t len, size_t* bytes_received) {
    ssize_t result;

    if (!conn || !buffer || !bytes_received) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    if (!conn->connected || conn->socket == FTN_INVALID_SOCKET) {
        return FTN_ERROR_NOT_CONNECTED;
    }

    result = recv(conn->socket, (char*)buffer, len, 0);
    if (result == -1) {
        int error = ftn_net_get_socket_error();
#ifdef _WIN32
        if (error == WSAEWOULDBLOCK) {
#else
        if (error == EAGAIN || error == EWOULDBLOCK) {
#endif
            *bytes_received = 0;
            return FTN_ERROR_WOULD_BLOCK;
        }
        return FTN_ERROR_NETWORK;
    }

    if (result == 0) {
        /* Connection closed by peer */
        conn->connected = 0;
        *bytes_received = 0;
        return FTN_ERROR_CONNECTION_CLOSED;
    }

    *bytes_received = (size_t)result;
    conn->bytes_received += *bytes_received;
    return FTN_OK;
}

ftn_error_t ftn_net_send_all(ftn_net_connection_t* conn, const void* data, size_t len) {
    const char* ptr = (const char*)data;
    size_t remaining = len;

    while (remaining > 0) {
        size_t bytes_sent;
        ftn_error_t result = ftn_net_send(conn, ptr, remaining, &bytes_sent);

        if (result == FTN_ERROR_WOULD_BLOCK) {
            /* Wait and retry */
            continue;
        }

        if (result != FTN_OK) {
            return result;
        }

        ptr += bytes_sent;
        remaining -= bytes_sent;
    }

    return FTN_OK;
}

ftn_error_t ftn_net_recv_all(ftn_net_connection_t* conn, void* buffer, size_t len) {
    char* ptr = (char*)buffer;
    size_t remaining = len;

    while (remaining > 0) {
        size_t bytes_received;
        ftn_error_t result = ftn_net_recv(conn, ptr, remaining, &bytes_received);

        if (result == FTN_ERROR_WOULD_BLOCK) {
            /* Wait and retry */
            continue;
        }

        if (result != FTN_OK) {
            return result;
        }

        if (bytes_received == 0) {
            return FTN_ERROR_CONNECTION_CLOSED;
        }

        ptr += bytes_received;
        remaining -= bytes_received;
    }

    return FTN_OK;
}

/* Non-blocking I/O */
ftn_error_t ftn_net_set_non_blocking(ftn_net_connection_t* conn, int non_blocking) {
    if (!conn) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    if (conn->socket == FTN_INVALID_SOCKET) {
        return FTN_ERROR_NOT_CONNECTED;
    }

    {
        ftn_error_t result = ftn_net_set_socket_non_blocking(conn->socket, non_blocking);
        if (result == FTN_OK) {
            conn->non_blocking = non_blocking;
        }

        return result;
    }
}

ftn_error_t ftn_net_select(ftn_net_connection_t** read_conns, size_t read_count,
                          ftn_net_connection_t** write_conns, size_t write_count,
                          int timeout_ms) {
    fd_set read_fds, write_fds;
    struct timeval tv;
    struct timeval* tv_ptr = NULL;
    ftn_socket_t max_fd = 0;
    size_t i;
    int result;

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);

    /* Setup read file descriptors */
    if (read_conns && read_count > 0) {
        for (i = 0; i < read_count; i++) {
            if (read_conns[i] && read_conns[i]->socket != FTN_INVALID_SOCKET) {
                FD_SET(read_conns[i]->socket, &read_fds);
                if (read_conns[i]->socket > max_fd) {
                    max_fd = read_conns[i]->socket;
                }
            }
        }
    }

    /* Setup write file descriptors */
    if (write_conns && write_count > 0) {
        for (i = 0; i < write_count; i++) {
            if (write_conns[i] && write_conns[i]->socket != FTN_INVALID_SOCKET) {
                FD_SET(write_conns[i]->socket, &write_fds);
                if (write_conns[i]->socket > max_fd) {
                    max_fd = write_conns[i]->socket;
                }
            }
        }
    }

    /* Setup timeout */
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tv_ptr = &tv;
    }

    /* Perform select */
    result = select(max_fd + 1, &read_fds, &write_fds, NULL, tv_ptr);
    if (result == -1) {
        return FTN_ERROR_NETWORK;
    }

    if (result == 0) {
        return FTN_ERROR_TIMEOUT;
    }

    return FTN_OK;
}

/* Server operations */
ftn_net_server_t* ftn_net_listen(int port, const char* bind_address, int max_connections) {
    ftn_net_server_t* server;
    struct sockaddr_in addr;
    ftn_socket_t sock;
    int reuse = 1;

    if (port <= 0 || port > 65535 || max_connections <= 0) {
        return NULL;
    }

    if (!ftn_net_initialized) {
        if (ftn_net_init() != FTN_OK) {
            return NULL;
        }
    }

    /* Allocate server structure */
    server = malloc(sizeof(ftn_net_server_t));
    if (!server) {
        return NULL;
    }

    memset(server, 0, sizeof(ftn_net_server_t));
    server->socket = FTN_INVALID_SOCKET;
    server->port = port;
    server->max_connections = max_connections;

    if (bind_address) {
        server->bind_address = malloc(strlen(bind_address) + 1);
        if (!server->bind_address) {
            free(server);
            return NULL;
        }
        strcpy(server->bind_address, bind_address);
    }

    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == FTN_INVALID_SOCKET) {
        ftn_net_server_free(server);
        return NULL;
    }

    /* Set socket options */
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
        ftn_net_close_socket(sock);
        ftn_net_server_free(server);
        return NULL;
    }

    /* Setup bind address */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (bind_address) {
        addr.sin_addr.s_addr = inet_addr(bind_address);
    } else {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    /* Bind socket */
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ftn_net_close_socket(sock);
        ftn_net_server_free(server);
        return NULL;
    }

    /* Start listening */
    if (listen(sock, max_connections) < 0) {
        ftn_net_close_socket(sock);
        ftn_net_server_free(server);
        return NULL;
    }

    server->socket = sock;
    server->listening = 1;

    return server;
}

ftn_net_connection_t* ftn_net_accept(ftn_net_server_t* server, int timeout_ms) {
    ftn_net_connection_t* conn;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    ftn_socket_t client_sock;

    if (!server || !server->listening) {
        return NULL;
    }

    /* Handle timeout */
    if (timeout_ms >= 0) {
        fd_set read_fds;
        struct timeval tv;

        FD_ZERO(&read_fds);
        FD_SET(server->socket, &read_fds);

        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        {
            int result = select(server->socket + 1, &read_fds, NULL, NULL, &tv);
            if (result <= 0) {
                return NULL;
            }
        }
    }

    /* Accept connection */
    client_sock = accept(server->socket, (struct sockaddr*)&client_addr, &addr_len);
    if (client_sock == FTN_INVALID_SOCKET) {
        return NULL;
    }

    /* Create connection structure */
    conn = malloc(sizeof(ftn_net_connection_t));
    if (!conn) {
        ftn_net_close_socket(client_sock);
        return NULL;
    }

    memset(conn, 0, sizeof(ftn_net_connection_t));
    conn->socket = client_sock;
    conn->connected = 1;
    conn->connect_time = time(NULL);

    /* Get client hostname */
    {
        char* client_ip = inet_ntoa(client_addr.sin_addr);
        if (client_ip) {
            conn->hostname = malloc(strlen(client_ip) + 1);
            if (conn->hostname) {
                strcpy(conn->hostname, client_ip);
            }
        }
    }
    conn->port = ntohs(client_addr.sin_port);

    return conn;
}

void ftn_net_server_free(ftn_net_server_t* server) {
    if (!server) {
        return;
    }

    if (server->socket != FTN_INVALID_SOCKET) {
        ftn_net_close_socket(server->socket);
    }

    if (server->bind_address) {
        free(server->bind_address);
    }

    free(server);
}

/* Socket options */
ftn_error_t ftn_net_set_keepalive(ftn_net_connection_t* conn, int enable) {
    if (!conn || conn->socket == FTN_INVALID_SOCKET) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    if (setsockopt(conn->socket, SOL_SOCKET, SO_KEEPALIVE, (const char*)&enable, sizeof(enable)) < 0) {
        return FTN_ERROR_NETWORK;
    }

    return FTN_OK;
}

ftn_error_t ftn_net_set_nodelay(ftn_net_connection_t* conn, int enable) {
    if (!conn || conn->socket == FTN_INVALID_SOCKET) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    if (setsockopt(conn->socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&enable, sizeof(enable)) < 0) {
        return FTN_ERROR_NETWORK;
    }

    return FTN_OK;
}

ftn_error_t ftn_net_set_timeout(ftn_net_connection_t* conn, int timeout_ms) {
    if (!conn || conn->socket == FTN_INVALID_SOCKET) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

#ifdef _WIN32
    DWORD timeout = timeout_ms;
    if (setsockopt(conn->socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0 ||
        setsockopt(conn->socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
        return FTN_ERROR_NETWORK;
    }
#else
    {
        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;

        if (setsockopt(conn->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
            setsockopt(conn->socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
            return FTN_ERROR_NETWORK;
        }
    }
#endif

    return FTN_OK;
}

/* Utility functions */
ftn_error_t ftn_net_resolve_hostname(const char* hostname, char* ip_buffer, size_t buffer_size) {
    struct hostent* host;

    if (!hostname || !ip_buffer || buffer_size == 0) {
        return FTN_ERROR_INVALID_PARAMETER;
    }

    if (!ftn_net_initialized) {
        if (ftn_net_init() != FTN_OK) {
            return FTN_ERROR_NETWORK;
        }
    }

    host = gethostbyname(hostname);
    if (!host) {
        return FTN_ERROR_NETWORK;
    }

    {
        char* ip = inet_ntoa(*((struct in_addr*)host->h_addr_list[0]));
        if (!ip) {
            return FTN_ERROR_NETWORK;
        }

        if (strlen(ip) >= buffer_size) {
            return FTN_ERROR_BUFFER_TOO_SMALL;
        }

        strcpy(ip_buffer, ip);
        return FTN_OK;
    }
}

const char* ftn_net_get_error_string(ftn_error_t error) {
    switch (error) {
        case FTN_OK:
            return "Success";
        case FTN_ERROR_INVALID_PARAMETER:
            return "Invalid parameter";
        case FTN_ERROR_NETWORK:
            return "Network error";
        case FTN_ERROR_NOT_CONNECTED:
            return "Not connected";
        case FTN_ERROR_CONNECTION_CLOSED:
            return "Connection closed";
        case FTN_ERROR_WOULD_BLOCK:
            return "Operation would block";
        case FTN_ERROR_TIMEOUT:
            return "Operation timed out";
        case FTN_ERROR_BUFFER_TOO_SMALL:
            return "Buffer too small";
        default:
            return "Unknown error";
    }
}

int ftn_net_is_connected(ftn_net_connection_t* conn) {
    if (!conn) {
        return 0;
    }

    return conn->connected && conn->socket != FTN_INVALID_SOCKET;
}

/* Error handling */
ftn_error_t ftn_net_get_last_error(void) {
    int error = ftn_net_get_socket_error();

#ifdef _WIN32
    switch (error) {
        case WSAEWOULDBLOCK:
            return FTN_ERROR_WOULD_BLOCK;
        case WSAECONNRESET:
        case WSAECONNABORTED:
        case WSAENOTCONN:
            return FTN_ERROR_CONNECTION_CLOSED;
        case WSAETIMEDOUT:
            return FTN_ERROR_TIMEOUT;
        default:
            return FTN_ERROR_NETWORK;
    }
#else
    switch (error) {
        case EAGAIN:
#if EAGAIN != EWOULDBLOCK
        case EWOULDBLOCK:
#endif
            return FTN_ERROR_WOULD_BLOCK;
        case ECONNRESET:
        case ECONNABORTED:
        case ENOTCONN:
        case EPIPE:
            return FTN_ERROR_CONNECTION_CLOSED;
        case ETIMEDOUT:
            return FTN_ERROR_TIMEOUT;
        default:
            return FTN_ERROR_NETWORK;
    }
#endif
}

void ftn_net_clear_error(void) {
#ifdef _WIN32
    WSASetLastError(0);
#else
    errno = 0;
#endif
}