/* M2 TLS 1.3 terminate — OpenSSL via dlopen (no libssl-dev link dep). */
#ifndef LI_RT_TLS_H
#define LI_RT_TLS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LI_HTTPD_MAX_CONN_TLS 512

/* 0=plain 1=tls+http/1.1 2=tls+h2 */
int32_t httpd_tls_slot_proto(int32_t slot);
void* httpd_tls_slot_ssl(int32_t slot);

int32_t httpd_tls_runtime_wanted(void);
int32_t httpd_tls_runtime_ready(void);

/* Load cert/key from dir/fullchain.pem + privkey.pem; enable ALPN h2 when http2_on. */
int32_t httpd_tls_global_init(const char* cert_dir, int32_t http2_on);

void httpd_tls_global_shutdown(void);

/* Non-blocking TLS accept after TCP accept; sets slot proto. Returns 0 ok, -1 fail. */
int32_t httpd_tls_handshake_slot(int32_t slot, int32_t fd);

void httpd_tls_free_slot(int32_t slot);

/* recv/send wrappers — use when httpd_tls_slot_proto(slot) != 0 */
ssize_t httpd_tls_read_slot(int32_t slot, void* buf, size_t cap);
ssize_t httpd_tls_write_slot(int32_t slot, const void* buf, size_t len);
ssize_t httpd_tls_write_fd(int32_t fd, const void* buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* LI_RT_TLS_H */
