/* M2 minimal HTTP/2 server (ALPN h2) — static-file GET smoke only. */
#ifndef LI_RT_H2_H
#define LI_RT_H2_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Drive H2 session for one client slot (TLS already up, proto=h2). Returns 0 handled, -1 close. */
int32_t httpd_h2_serve_slot(int32_t epfd, int32_t slot);

#ifdef __cplusplus
}
#endif

#endif /* LI_RT_H2_H */
