#pragma once

#include <stdint.h>

/* Trusted syscall seam stubs for li-net / li-httpd (P0 — no real epoll yet). */

int32_t tcp_listen(int32_t port);
int32_t tcp_accept(int32_t listen_fd);
int32_t tcp_send(int32_t conn_fd, const char* data);
const char* tcp_recv(int32_t conn_fd, int32_t max_bytes);
void tcp_close(int32_t conn_fd);
