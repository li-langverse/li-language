/* Minimal li-httpd [routes] TOML loader + matcher (M1 — trusted C until std TOML ships). */

#include "li_rt.h"
#include "li_rt_net.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LI_HTTPD_MAX_ROUTES 64
#define LI_HTTPD_METHOD_MAX 16
#define LI_HTTPD_PATH_MAX 512
#define LI_HTTPD_ACTION_MAX 128
#define LI_HTTPD_NAME_MAX 64
#define LI_HTTPD_HEADERS_MAX 128
#define LI_HTTPD_FILE_MAX (256 * 1024)

typedef enum {
  LI_PATH_EXACT = 0,
  LI_PATH_PREFIX = 1,
  LI_PATH_PREFIX_STRIP = 2,
} LiPathKind;

typedef struct {
  char name[LI_HTTPD_NAME_MAX];
  char method[LI_HTTPD_METHOD_MAX];
  char path[LI_HTTPD_PATH_MAX];
  LiPathKind path_kind;
  char action[LI_HTTPD_ACTION_MAX];
  char headers[LI_HTTPD_HEADERS_MAX]; /* space-separated key=value (lowercase keys) */
  int32_t require_traceparent;        /* M1.5 OTel — route extra require=traceparent */
  int32_t require_websocket;          /* M2 — route extra require=websocket */
  int32_t priority;
  int32_t route_id; /* 1-based id after load */
} LiHttpdRoute;

/* M1.5 oracle / validate-config state (set when [limits] stream keys present). */
static int32_t g_m15_stream_idle_sec = 0;
static int32_t g_m15_stream_max_sec = 0;
static int32_t g_m15_concurrent_streams = 0;

/* M1.5 leak_censor oracle state ([leak_censor] in TOML). */
static int32_t g_leak_censor_enabled = 0;
static int32_t g_leak_censor_deny_path_count = 0;
static int32_t g_leak_censor_pattern_openai = 0;
static int32_t g_leak_censor_pattern_jwt = 0;
static int32_t g_leak_censor_pattern_pem = 0;

/* M1.5 TLS auto oracle ([server.tls] in TOML). */
static int32_t g_tls_enabled = 0;
static int32_t g_tls_mode = 0; /* 1=manual 2=self_signed 3=lets_encrypt */
static int32_t g_tls_le_domain_count = 0;
static int32_t g_tls_renew_before_days = 30;
static int32_t g_tls_self_signed_dev = 0;
static char g_tls_le_email[128];

/* M2 scale oracle ([server.http2], [route.queue], [circuit_breaker], [webhooks]). */
static int32_t g_m2_enabled = 0;
static int32_t g_m2_tls_terminate = 0;
static int32_t g_m2_http2_enabled = 0;
static int32_t g_m2_http2_max_streams = 0;
static int32_t g_m2_queue_max_depth = 0;
static int32_t g_m2_queue_max_wait_sec = 0;
static int32_t g_m2_queue_retry_after_sec = 1;
static int32_t g_m2_cb_error_threshold = 0;
static int32_t g_m2_cb_window_sec = 0;
static int32_t g_m2_cb_open_duration_sec = 0;
static int32_t g_m2_cb_half_open_probes = 0;
static int32_t g_m2_webhook_allow_count = 0;
static char g_m2_webhook_allow[8][256];

/* M3 optional — L4 stream config + token-budget hooks (RFC; relay deferred). */
static int32_t g_m3_enabled = 0;
static int32_t g_m3_l4_enabled = 0;
static int32_t g_m3_l4_listen_port = 0;
static int32_t g_m3_l4_upstream_port = 0;
static int32_t g_m3_l4_max_connections = 0;
static char g_m3_l4_listen_host[128];
static char g_m3_l4_upstream_host[128];
static int32_t g_m3_token_budget_enabled = 0;
static int32_t g_m3_token_budget_max = 0;
static int32_t g_m3_token_budget_reject_over = 1;
static char g_m3_token_budget_header[64] = "x-token-budget";

static LiHttpdRoute g_routes[LI_HTTPD_MAX_ROUTES];
static int32_t g_route_count = 0;

/* 1=io 2=route_key 3=path_traversal 4=overlap 5=parse */
static int32_t g_httpd_last_kind = 0;
static char g_httpd_last_msg[512];

static void httpd_set_error(int32_t kind, const char* msg) {
  g_httpd_last_kind = kind;
  if (msg == NULL) {
    g_httpd_last_msg[0] = '\0';
    return;
  }
  strncpy(g_httpd_last_msg, msg, sizeof(g_httpd_last_msg) - 1);
  g_httpd_last_msg[sizeof(g_httpd_last_msg) - 1] = '\0';
}

int32_t li_rt_httpd_last_error_kind(void) { return g_httpd_last_kind; }

const char* li_rt_httpd_last_error_message(void) { return g_httpd_last_msg; }

int32_t li_rt_httpd_route_count(void) { return g_route_count; }

static void httpd_clear_routes(void) {
  g_route_count = 0;
  memset(g_routes, 0, sizeof(g_routes));
}

static void slug_route_name(const char* method, const char* path, char* out, size_t out_len) {
  char path_work[LI_HTTPD_PATH_MAX];
  const char* slug_path = path;
  size_t plen = strlen(path);
  if (plen >= 3 && strcmp(path + plen - 3, "/**") == 0) {
    snprintf(path_work, sizeof(path_work), "%.*s_rest", (int)(plen - 3), path);
    slug_path = path_work;
  } else if (plen >= 2 && strcmp(path + plen - 2, "/*") == 0) {
    snprintf(path_work, sizeof(path_work), "%.*s_wild", (int)(plen - 2), path);
    slug_path = path_work;
  }
  size_t n = 0;
  const char* p = method;
  while (*p && n + 1 < out_len) {
    char c = *p++;
    if (c >= 'A' && c <= 'Z') {
      c = (char)(c - 'A' + 'a');
    }
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      out[n++] = c;
    } else {
      out[n++] = '_';
    }
  }
  if (n + 1 < out_len) {
    out[n++] = '_';
  }
  for (p = slug_path; *p && n + 1 < out_len; p++) {
    char c = *p;
    if (c == '/') {
      if (n > 0 && out[n - 1] != '_') {
        out[n++] = '_';
      }
      continue;
    }
    if (c == '*') {
      if (n + 4 < out_len) {
        out[n++] = 'w';
        out[n++] = 'i';
        out[n++] = 'l';
        out[n++] = 'd';
      }
      continue;
    }
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      if (c >= 'A' && c <= 'Z') {
        c = (char)(c - 'A' + 'a');
      }
      out[n++] = c;
    } else {
      out[n++] = '_';
    }
  }
  while (n > 0 && out[n - 1] == '_') {
    n--;
  }
  if (n == 0 && out_len > 4) {
    memcpy(out, "route", 5);
    n = 5;
  }
  out[n] = '\0';
}

static int parse_path_kind(const char* raw_path, char* norm_path, size_t norm_len, LiPathKind* kind) {
  size_t len = strlen(raw_path);
  if (len >= norm_len) {
    return -1;
  }
  memcpy(norm_path, raw_path, len + 1);
  *kind = LI_PATH_EXACT;
  if (len >= 3 && strcmp(raw_path + len - 3, "/**") == 0) {
    norm_path[len - 3] = '\0';
    *kind = LI_PATH_PREFIX_STRIP;
    return 0;
  }
  if (len >= 2 && strcmp(raw_path + len - 2, "/*") == 0) {
    norm_path[len - 2] = '\0';
    *kind = LI_PATH_PREFIX;
    return 0;
  }
  return 0;
}

static int path_has_traversal(const char* raw_path) {
  if (strstr(raw_path, "..") != NULL) {
    return 1;
  }
  const char* p = raw_path;
  while ((p = strstr(p, "//")) != NULL) {
    if (p > raw_path + 1 && p[-2] != ':' && p[-1] != '/') {
      return 1;
    }
    p += 2;
  }
  return 0;
}

/* M1 ingress allowlist — must match scripts/httpd_config.py */
static const char* const k_ingress_header_allow[] = {
    "authorization", "content-type", "accept", "traceparent", "x-request-id",
    "x-agent-id", "x-model", "idempotency-key", NULL};

static int header_name_allowed(const char* name) {
  for (size_t i = 0; k_ingress_header_allow[i] != NULL; i++) {
    if (strcmp(name, k_ingress_header_allow[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

static int validate_ingress_header_name(const char* name) {
  static const char* const hop_by_hop[] = {
      "connection", "keep-alive", "proxy-authenticate", "proxy-authorization",
      "te", "trailer", "transfer-encoding", "upgrade", NULL};
  for (size_t i = 0; hop_by_hop[i] != NULL; i++) {
    if (strcmp(name, hop_by_hop[i]) == 0) {
      httpd_set_error(2, "hop-by-hop header not allowed in route extras");
      return -1;
    }
  }
  if (strncmp(name, "x-upstream-", 11) == 0 || strncmp(name, "x-route-", 8) == 0) {
    httpd_set_error(2, "forbidden header prefix in route extras");
    return -1;
  }
  if (!header_name_allowed(name)) {
    httpd_set_error(2, "header not in ingress allowlist (M1 route extras)");
    return -1;
  }
  return 0;
}

static int append_route_header(LiHttpdRoute* out, const char* key, const char* val) {
  char pair[96];
  int n = snprintf(pair, sizeof(pair), "%s=%s", key, val);
  if (n < 0 || (size_t)n >= sizeof(pair)) {
    return -1;
  }
  size_t cur = strlen(out->headers);
  if (cur > 0) {
    if (cur + 1 >= sizeof(out->headers)) {
      return -1;
    }
    out->headers[cur++] = ' ';
    out->headers[cur] = '\0';
  }
  if (cur + (size_t)n + 1 >= sizeof(out->headers)) {
    return -1;
  }
  strcat(out->headers, pair);
  return 0;
}

static int parse_route_extras(const char* extras, LiHttpdRoute* out) {
  char buf[LI_HTTPD_HEADERS_MAX];
  char key[64];
  char val[64];
  size_t n = strlen(extras);
  if (n == 0) {
    return 0;
  }
  if (n >= sizeof(buf)) {
    return -1;
  }
  memcpy(buf, extras, n + 1);
  char* save = NULL;
  for (char* tok = strtok_r(buf, " \t", &save); tok != NULL; tok = strtok_r(NULL, " \t", &save)) {
    char* eq = strchr(tok, '=');
    if (eq == NULL || eq == tok) {
      httpd_set_error(2, "invalid route extra: expected key=value");
      return -1;
    }
    *eq = '\0';
    strncpy(key, tok, sizeof(key) - 1);
    key[sizeof(key) - 1] = '\0';
    strncpy(val, eq + 1, sizeof(val) - 1);
    val[sizeof(val) - 1] = '\0';
    for (char* k = key; *k; k++) {
      if (*k >= 'A' && *k <= 'Z') {
        *k = (char)(*k - 'A' + 'a');
      }
    }
    if (key[0] == '\0' || val[0] == '\0') {
      httpd_set_error(2, "invalid route extra: expected key=value");
      return -1;
    }
    if (strcmp(key, "require") == 0) {
      if (strcmp(val, "traceparent") == 0) {
        out->require_traceparent = 1;
        continue;
      }
      if (strcmp(val, "websocket") == 0) {
        out->require_websocket = 1;
        continue;
      }
      httpd_set_error(2, "unsupported require= value (allowed: traceparent, websocket)");
      return -1;
    }
    if (validate_ingress_header_name(key) != 0) {
      return -1;
    }
    if (append_route_header(out, key, val) != 0) {
      return -1;
    }
  }
  return 0;
}

static int parse_route_key_value(const char* key, const char* action, int32_t priority, LiHttpdRoute* out) {
  char method[LI_HTTPD_METHOD_MAX];
  char raw_path[LI_HTTPD_PATH_MAX];
  const char* sp;
  const char* p = key;
  size_t mi = 0;
  while (*p && !isspace((unsigned char)*p) && mi + 1 < sizeof(method)) {
    method[mi++] = *p++;
  }
  method[mi] = '\0';
  if (mi == 0) {
    httpd_set_error(2, "invalid route key: expected METHOD /path");
    return -1;
  }
  while (*p && isspace((unsigned char)*p)) {
    p++;
  }
  if (*p != '/') {
    httpd_set_error(2, "invalid route key: path must start with /");
    return -1;
  }
  sp = p;
  while (*p && *p != '#' && !isspace((unsigned char)*p)) {
    p++;
  }
  size_t plen = (size_t)(p - sp);
  if (plen == 0 || plen >= sizeof(raw_path)) {
    return -1;
  }
  memcpy(raw_path, sp, plen);
  raw_path[plen] = '\0';
  if (path_has_traversal(raw_path)) {
    httpd_set_error(3, "path must not contain .. or //");
    return -1;
  }
  memset(out, 0, sizeof(*out));
  strncpy(out->method, method, sizeof(out->method) - 1);
  if (parse_path_kind(raw_path, out->path, sizeof(out->path), &out->path_kind) != 0) {
    return -1;
  }
  strncpy(out->action, action, sizeof(out->action) - 1);
  slug_route_name(method, raw_path, out->name, sizeof(out->name));
  out->priority = priority;
  while (*p && isspace((unsigned char)*p)) {
    p++;
  }
  if (*p != '\0' && *p != '#') {
    if (parse_route_extras(p, out) != 0) {
      return -1;
    }
  }
  return 0;
}

static int routes_overlap(const LiHttpdRoute* a, const LiHttpdRoute* b) {
  if (strcmp(a->method, b->method) != 0) {
    return 0;
  }
  if (a->path_kind == LI_PATH_EXACT && b->path_kind == LI_PATH_EXACT) {
    return strcmp(a->path, b->path) == 0;
  }
  if (a->path_kind != LI_PATH_EXACT && b->path_kind != LI_PATH_EXACT) {
    size_t la = strlen(a->path);
    size_t lb = strlen(b->path);
    const char* pa = a->path;
    const char* pb = b->path;
    if (strcmp(pa, pb) == 0) {
      return 1;
    }
    if (la <= lb && strncmp(pb, pa, la) == 0 && (pb[la] == '\0' || pb[la] == '/')) {
      return 1;
    }
    if (lb <= la && strncmp(pa, pb, lb) == 0 && (pa[lb] == '\0' || pa[lb] == '/')) {
      return 1;
    }
  }
  return strcmp(a->path, b->path) == 0;
}

static int validate_routes(void) {
  for (int32_t i = 0; i < g_route_count; i++) {
    for (int32_t j = i + 1; j < g_route_count; j++) {
      if (g_routes[i].priority != g_routes[j].priority && routes_overlap(&g_routes[i], &g_routes[j])) {
        continue;
      }
      if (routes_overlap(&g_routes[i], &g_routes[j])) {
        char msg[256];
        snprintf(msg, sizeof(msg), "overlapping routes: %s vs %s", g_routes[i].name, g_routes[j].name);
        httpd_set_error(4, msg);
        return -1;
      }
    }
  }
  return 0;
}

static void trim_line(char* line) {
  size_t n = strlen(line);
  while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' || isspace((unsigned char)line[n - 1]))) {
    line[--n] = '\0';
  }
  char* p = line;
  while (*p && isspace((unsigned char)*p)) {
    p++;
  }
  if (p != line) {
    memmove(line, p, strlen(p) + 1);
  }
}

/* M1: proxy routes require global limits.rate_limit_rps (agent gateway policy). */
static int parse_duration_sec_value(const char* raw, int* out_sec) {
  if (raw == NULL || out_sec == NULL) {
    return -1;
  }
  while (*raw && isspace((unsigned char)*raw)) {
    raw++;
  }
  if (*raw == '"') {
    raw++;
  }
  char* end = NULL;
  long n = strtol(raw, &end, 10);
  if (n < 1 || n > 86400) {
    return -1;
  }
  if (end && (*end == 's' || *end == 'S')) {
    end++;
  }
  while (end && *end && isspace((unsigned char)*end)) {
    end++;
  }
  if (end && *end == '"') {
    end++;
  }
  while (end && *end && isspace((unsigned char)*end)) {
    end++;
  }
  if (end && *end != '\0') {
    return -1;
  }
  *out_sec = (int)n;
  return 0;
}

static int parse_limits_m15_stream(const char* text) {
  const char* sec = strstr(text, "[limits]");
  if (sec == NULL) {
    return 0;
  }
  char line[256];
  const char* p = strchr(sec, '\n');
  int idle = 0;
  int max_d = 0;
  int conc = 0;
  if (p == NULL) {
    return 0;
  }
  while (*p) {
    const char* line_end = strchr(p, '\n');
    size_t len = line_end ? (size_t)(line_end - p) : strlen(p);
    if (len >= sizeof(line)) {
      return -1;
    }
    memcpy(line, p, len);
    line[len] = '\0';
    trim_line(line);
    p = line_end ? line_end + 1 : p + len;
    if (line[0] == '[') {
      break;
    }
    if (line[0] == '\0' || line[0] == '#') {
      continue;
    }
    const char* eq = strchr(line, '=');
    if (eq == NULL) {
      continue;
    }
    char key[64];
    size_t klen = (size_t)(eq - line);
    if (klen >= sizeof(key)) {
      return -1;
    }
    memcpy(key, line, klen);
    key[klen] = '\0';
    trim_line(key);
    eq++;
    while (*eq && isspace((unsigned char)*eq)) {
      eq++;
    }
    if (strcmp(key, "stream_idle_timeout") == 0) {
      if (parse_duration_sec_value(eq, &idle) != 0) {
        return -1;
      }
    } else if (strcmp(key, "stream_max_duration") == 0) {
      if (parse_duration_sec_value(eq, &max_d) != 0) {
        return -1;
      }
    } else if (strcmp(key, "concurrent_streams") == 0) {
      conc = atoi(eq);
    }
  }
  if (idle > 0 || conc > 0) {
    if (idle <= 0 || max_d <= 0 || conc <= 0) {
      httpd_set_error(5, "M1.5 limits require stream_idle_timeout, stream_max_duration, concurrent_streams");
      return -1;
    }
    if (max_d < idle) {
      httpd_set_error(5, "limits.stream_max_duration must be >= stream_idle_timeout");
      return -1;
    }
    g_m15_stream_idle_sec = idle;
    g_m15_stream_max_sec = max_d;
    g_m15_concurrent_streams = conc;
  }
  return 0;
}

static int parse_leak_censor_table(const char* text) {
  int enabled = 0;
  int deny_count = 0;
  int pat_openai = 0;
  int pat_jwt = 0;
  int pat_pem = 0;
  const char* scan = text;
  while (scan && *scan) {
    const char* sec = strstr(scan, "[leak_censor");
    if (sec == NULL) {
      break;
    }
    const char* close = strchr(sec, ']');
    if (close == NULL) {
      break;
    }
    char line[256];
    const char* p = strchr(close, '\n');
    if (p == NULL) {
      break;
    }
    while (*p) {
      const char* line_end = strchr(p, '\n');
      size_t len = line_end ? (size_t)(line_end - p) : strlen(p);
      if (len >= sizeof(line)) {
        return -1;
      }
      memcpy(line, p, len);
      line[len] = '\0';
      trim_line(line);
      p = line_end ? line_end + 1 : p + len;
      if (line[0] == '[') {
        break;
      }
      if (line[0] == '\0' || line[0] == '#') {
        continue;
      }
      const char* eq = strchr(line, '=');
      if (eq == NULL) {
        continue;
      }
      char key[64];
      size_t klen = (size_t)(eq - line);
      if (klen >= sizeof(key)) {
        return -1;
      }
      memcpy(key, line, klen);
      key[klen] = '\0';
      trim_line(key);
      eq++;
      while (*eq && isspace((unsigned char)*eq)) {
        eq++;
      }
      if (strcmp(key, "enabled") == 0) {
        if (strcmp(eq, "true") == 0 || strcmp(eq, "1") == 0) {
          enabled = 1;
        }
      } else if (strcmp(key, "deny_paths") == 0) {
        const char* q = eq;
        while ((q = strchr(q, '"')) != NULL) {
          deny_count++;
          q++;
        }
      } else if (strcmp(key, "allow") == 0) {
        if (strstr(eq, "openai_sk") != NULL) {
          pat_openai = 1;
        }
        if (strstr(eq, "jwt_bearer") != NULL) {
          pat_jwt = 1;
        }
        if (strstr(eq, "pem_private") != NULL) {
          pat_pem = 1;
        }
      }
    }
    scan = p;
  }
  if (deny_count == 0) {
    const char* dp = strstr(text, "deny_paths");
    if (dp != NULL) {
      const char* eq = strchr(dp, '=');
      if (eq != NULL) {
        while ((eq = strchr(eq, '"')) != NULL) {
          deny_count++;
          eq++;
        }
      }
    }
  }
  g_leak_censor_enabled = enabled;
  g_leak_censor_deny_path_count = deny_count;
  g_leak_censor_pattern_openai = pat_openai;
  g_leak_censor_pattern_jwt = pat_jwt;
  g_leak_censor_pattern_pem = pat_pem;
  return 0;
}

static int parse_int_field(const char* eq) {
  while (*eq && isspace((unsigned char)*eq)) {
    eq++;
  }
  return atoi(eq);
}

static int parse_m2_section_table(const char* text, const char* section,
                                  int (*on_line)(const char* key, const char* val, void* ctx), void* ctx) {
  const char* sec = strstr(text, section);
  if (sec == NULL) {
    return 0;
  }
  const char* close = strchr(sec, ']');
  if (close == NULL) {
    return 0;
  }
  char line[512];
  const char* p = strchr(close, '\n');
  if (p == NULL) {
    return 0;
  }
  while (*p) {
    const char* line_end = strchr(p, '\n');
    size_t len = line_end ? (size_t)(line_end - p) : strlen(p);
    if (len >= sizeof(line)) {
      return -1;
    }
    memcpy(line, p, len);
    line[len] = '\0';
    trim_line(line);
    p = line_end ? line_end + 1 : p + len;
    if (line[0] == '[') {
      break;
    }
    if (line[0] == '\0' || line[0] == '#') {
      continue;
    }
    const char* eq = strchr(line, '=');
    if (eq == NULL) {
      continue;
    }
    char key[64];
    size_t klen = (size_t)(eq - line);
    if (klen >= sizeof(key)) {
      return -1;
    }
    memcpy(key, line, klen);
    key[klen] = '\0';
    trim_line(key);
    eq++;
    if (on_line(key, eq, ctx) != 0) {
      return -1;
    }
  }
  return 0;
}

static int m2_http2_line(const char* key, const char* val, void* ctx) {
  (void)ctx;
  if (strcmp(key, "enabled") == 0) {
    if (strcmp(val, "true") == 0 || strcmp(val, "1") == 0) {
      g_m2_http2_enabled = 1;
      g_m2_enabled = 1;
    }
  } else if (strcmp(key, "max_concurrent_streams") == 0) {
    g_m2_http2_max_streams = parse_int_field(val);
    if (g_m2_http2_max_streams <= 0) {
      g_m2_http2_max_streams = 256;
    }
  }
  return 0;
}

static int m2_queue_line(const char* key, const char* val, void* ctx) {
  (void)ctx;
  if (strcmp(key, "max_depth") == 0) {
    g_m2_queue_max_depth = parse_int_field(val);
    g_m2_enabled = 1;
  } else if (strcmp(key, "max_wait") == 0) {
    g_m2_queue_max_wait_sec = li_rt_httpd_parse_duration_sec(val);
  } else if (strcmp(key, "retry_after") == 0) {
    int s = li_rt_httpd_parse_duration_sec(val);
    if (s > 0) {
      g_m2_queue_retry_after_sec = s;
    }
  }
  return 0;
}

static int m2_cb_line(const char* key, const char* val, void* ctx) {
  (void)ctx;
  if (strcmp(key, "error_threshold") == 0) {
    g_m2_cb_error_threshold = parse_int_field(val);
    g_m2_enabled = 1;
  } else if (strcmp(key, "window_sec") == 0) {
    g_m2_cb_window_sec = parse_int_field(val);
  } else if (strcmp(key, "open_duration_sec") == 0) {
    g_m2_cb_open_duration_sec = parse_int_field(val);
  } else if (strcmp(key, "half_open_probes") == 0) {
    g_m2_cb_half_open_probes = parse_int_field(val);
  }
  return 0;
}

static int m2_tls_line(const char* key, const char* val, void* ctx) {
  (void)ctx;
  if (strcmp(key, "terminate") == 0) {
    if (strcmp(val, "true") == 0 || strcmp(val, "1") == 0) {
      g_m2_tls_terminate = 1;
      g_m2_enabled = 1;
    }
  }
  return 0;
}

static int m2_webhooks_line(const char* key, const char* val, void* ctx) {
  (void)ctx;
  if (strcmp(key, "allow") != 0) {
    return 0;
  }
  const char* q = val;
  while (g_m2_webhook_allow_count < 8 && (q = strchr(q, '"')) != NULL) {
    q++;
    const char* end = strchr(q, '"');
    if (end == NULL) {
      break;
    }
    size_t n = (size_t)(end - q);
    if (n >= sizeof(g_m2_webhook_allow[0])) {
      return -1;
    }
    memcpy(g_m2_webhook_allow[g_m2_webhook_allow_count], q, n);
    g_m2_webhook_allow[g_m2_webhook_allow_count][n] = '\0';
    g_m2_webhook_allow_count++;
    g_m2_enabled = 1;
    q = end + 1;
  }
  return 0;
}

static int parse_m2_config(const char* text) {
  g_m2_enabled = 0;
  g_m2_tls_terminate = 0;
  g_m2_http2_enabled = 0;
  g_m2_http2_max_streams = 0;
  g_m2_queue_max_depth = 0;
  g_m2_queue_max_wait_sec = 0;
  g_m2_queue_retry_after_sec = 1;
  g_m2_cb_error_threshold = 0;
  g_m2_cb_window_sec = 0;
  g_m2_cb_open_duration_sec = 0;
  g_m2_cb_half_open_probes = 0;
  g_m2_webhook_allow_count = 0;

  int rc = parse_m2_section_table(text, "[server.tls]", m2_tls_line, NULL);
  if (rc != 0) {
    return rc;
  }
  rc = parse_m2_section_table(text, "[server.http2]", m2_http2_line, NULL);
  if (rc != 0) {
    return rc;
  }
  rc = parse_m2_section_table(text, "[route.queue]", m2_queue_line, NULL);
  if (rc != 0) {
    return rc;
  }
  rc = parse_m2_section_table(text, "[circuit_breaker]", m2_cb_line, NULL);
  if (rc != 0) {
    return rc;
  }
  rc = parse_m2_section_table(text, "[webhooks]", m2_webhooks_line, NULL);
  return rc;
}

static int m3_l4_line(const char* key, const char* val, void* ctx) {
  (void)ctx;
  if (strcmp(key, "enabled") == 0) {
    if (val[0] == '1' || strcasecmp(val, "true") == 0 || strcasecmp(val, "yes") == 0) {
      g_m3_l4_enabled = 1;
      g_m3_enabled = 1;
    }
  } else if (strcmp(key, "listen") == 0) {
    const char* colon = strrchr(val, ':');
    if (colon != NULL && colon > val) {
      size_t hlen = (size_t)(colon - val);
      if (hlen < sizeof(g_m3_l4_listen_host)) {
        memcpy(g_m3_l4_listen_host, val, hlen);
        g_m3_l4_listen_host[hlen] = '\0';
      }
      g_m3_l4_listen_port = parse_int_field(colon + 1);
    }
  } else if (strcmp(key, "upstream_host") == 0) {
    strncpy(g_m3_l4_upstream_host, val, sizeof(g_m3_l4_upstream_host) - 1);
    g_m3_l4_upstream_host[sizeof(g_m3_l4_upstream_host) - 1] = '\0';
  } else if (strcmp(key, "upstream_port") == 0) {
    g_m3_l4_upstream_port = parse_int_field(val);
  } else if (strcmp(key, "max_connections") == 0) {
    g_m3_l4_max_connections = parse_int_field(val);
  }
  return 0;
}

static int m3_token_budget_line(const char* key, const char* val, void* ctx) {
  (void)ctx;
  if (strcmp(key, "enabled") == 0) {
    if (val[0] == '1' || strcasecmp(val, "true") == 0 || strcasecmp(val, "yes") == 0) {
      g_m3_token_budget_enabled = 1;
      g_m3_enabled = 1;
    }
  } else if (strcmp(key, "header") == 0) {
    strncpy(g_m3_token_budget_header, val, sizeof(g_m3_token_budget_header) - 1);
    g_m3_token_budget_header[sizeof(g_m3_token_budget_header) - 1] = '\0';
  } else if (strcmp(key, "max_per_request") == 0) {
    g_m3_token_budget_max = parse_int_field(val);
  } else if (strcmp(key, "reject_over_cap") == 0) {
    g_m3_token_budget_reject_over =
        (val[0] == '0' || strcasecmp(val, "false") == 0 || strcasecmp(val, "no") == 0) ? 0 : 1;
  }
  return 0;
}

static int parse_m3_config(const char* text) {
  g_m3_enabled = 0;
  g_m3_l4_enabled = 0;
  g_m3_l4_listen_port = 0;
  g_m3_l4_upstream_port = 0;
  g_m3_l4_max_connections = 0;
  g_m3_l4_listen_host[0] = '\0';
  g_m3_l4_upstream_host[0] = '\0';
  g_m3_token_budget_enabled = 0;
  g_m3_token_budget_max = 0;
  g_m3_token_budget_reject_over = 1;
  strncpy(g_m3_token_budget_header, "x-token-budget", sizeof(g_m3_token_budget_header) - 1);
  g_m3_token_budget_header[sizeof(g_m3_token_budget_header) - 1] = '\0';

  int rc = parse_m2_section_table(text, "[server.l4_stream]", m3_l4_line, NULL);
  if (rc != 0) {
    return rc;
  }
  rc = parse_m2_section_table(text, "[limits.token_budget]", m3_token_budget_line, NULL);
  return rc;
}

static int tls_mode_from_str(const char* eq) {
  if (eq == NULL) {
    return 0;
  }
  if (strstr(eq, "manual") != NULL) {
    return 1;
  }
  if (strstr(eq, "self_signed") != NULL) {
    return 2;
  }
  if (strstr(eq, "lets_encrypt") != NULL) {
    return 3;
  }
  return 0;
}

static int parse_server_tls(const char* text) {
  int mode = 0;
  int renew_days = 30;
  int dev = 0;
  int domain_count = 0;
  char email[128];
  email[0] = '\0';

  const char* sec = strstr(text, "[server]");
  if (sec != NULL) {
    const char* close = strchr(sec, ']');
    if (close != NULL) {
      char line[256];
      const char* p = strchr(close, '\n');
      if (p != NULL) {
        while (*p) {
          const char* line_end = strchr(p, '\n');
          size_t len = line_end ? (size_t)(line_end - p) : strlen(p);
          if (len >= sizeof(line)) {
            return -1;
          }
          memcpy(line, p, len);
          line[len] = '\0';
          trim_line(line);
          p = line_end ? line_end + 1 : p + len;
          if (line[0] == '[') {
            break;
          }
          if (line[0] == '\0' || line[0] == '#') {
            continue;
          }
          const char* eq = strchr(line, '=');
          if (eq == NULL) {
            continue;
          }
          char key[64];
          size_t klen = (size_t)(eq - line);
          if (klen >= sizeof(key)) {
            return -1;
          }
          memcpy(key, line, klen);
          key[klen] = '\0';
          trim_line(key);
          eq++;
          while (*eq && isspace((unsigned char)*eq)) {
            eq++;
          }
          if (strcmp(key, "tls") == 0) {
            int m = tls_mode_from_str(eq);
            if (m > 0) {
              mode = m;
            }
          } else if (strcmp(key, "email") == 0) {
            strncpy(email, eq, sizeof(email) - 1);
            email[sizeof(email) - 1] = '\0';
            trim_line(email);
            if (email[0] == '"') {
              size_t el = strlen(email);
              if (el >= 2 && email[el - 1] == '"') {
                email[el - 1] = '\0';
                memmove(email, email + 1, el - 1);
              }
            }
          }
        }
      }
    }
  }

  const char* scan = text;
  while (scan && *scan) {
    const char* tls_sec = strstr(scan, "[server.tls");
    if (tls_sec == NULL) {
      break;
    }
    const char* close = strchr(tls_sec, ']');
    if (close == NULL) {
      break;
    }
    char line[256];
    const char* p = strchr(close, '\n');
    if (p == NULL) {
      break;
    }
    while (*p) {
      const char* line_end = strchr(p, '\n');
      size_t len = line_end ? (size_t)(line_end - p) : strlen(p);
      if (len >= sizeof(line)) {
        return -1;
      }
      memcpy(line, p, len);
      line[len] = '\0';
      trim_line(line);
      p = line_end ? line_end + 1 : p + len;
      if (line[0] == '[') {
        break;
      }
      if (line[0] == '\0' || line[0] == '#') {
        continue;
      }
      const char* eq = strchr(line, '=');
      if (eq == NULL) {
        continue;
      }
      char key[64];
      size_t klen = (size_t)(eq - line);
      if (klen >= sizeof(key)) {
        return -1;
      }
      memcpy(key, line, klen);
      key[klen] = '\0';
      trim_line(key);
      eq++;
      while (*eq && isspace((unsigned char)*eq)) {
        eq++;
      }
      if (strcmp(key, "mode") == 0) {
        int m = tls_mode_from_str(eq);
        if (m > 0) {
          mode = m;
        }
      } else if (strcmp(key, "renew_before") == 0) {
        renew_days = atoi(eq);
        if (renew_days <= 0) {
          const char* d = eq;
          while (*d && !isdigit((unsigned char)*d)) {
            d++;
          }
          renew_days = atoi(d);
        }
      }
    }
    scan = p;
  }

  const char* le_sec = strstr(text, "[server.tls.lets_encrypt]");
  if (le_sec == NULL) {
    le_sec = strstr(text, "server.tls.lets_encrypt");
  }
  if (le_sec != NULL) {
    const char* close = strchr(le_sec, ']');
    if (close != NULL) {
      char line[256];
      const char* p = strchr(close, '\n');
      if (p != NULL) {
        while (*p) {
          const char* line_end = strchr(p, '\n');
          size_t len = line_end ? (size_t)(line_end - p) : strlen(p);
          if (len >= sizeof(line)) {
            return -1;
          }
          memcpy(line, p, len);
          line[len] = '\0';
          trim_line(line);
          p = line_end ? line_end + 1 : p + len;
          if (line[0] == '[') {
            break;
          }
          if (line[0] == '\0' || line[0] == '#') {
            continue;
          }
          const char* eq = strchr(line, '=');
          if (eq == NULL) {
            continue;
          }
          char key[64];
          size_t klen = (size_t)(eq - line);
          if (klen >= sizeof(key)) {
            return -1;
          }
          memcpy(key, line, klen);
          key[klen] = '\0';
          trim_line(key);
          eq++;
          while (*eq && isspace((unsigned char)*eq)) {
            eq++;
          }
          if (strcmp(key, "email") == 0 && email[0] == '\0') {
            strncpy(email, eq, sizeof(email) - 1);
            email[sizeof(email) - 1] = '\0';
            trim_line(email);
          } else if (strcmp(key, "domains") == 0) {
            const char* q = eq;
            while ((q = strchr(q, '"')) != NULL) {
              domain_count++;
              q++;
            }
          }
        }
      }
    }
  }

  const char* ss_sec = strstr(text, "[server.tls.self_signed]");
  if (ss_sec != NULL) {
    const char* close = strchr(ss_sec, ']');
    if (close != NULL) {
      char line[256];
      const char* p = strchr(close, '\n');
      if (p != NULL) {
        while (*p) {
          const char* line_end = strchr(p, '\n');
          size_t len = line_end ? (size_t)(line_end - p) : strlen(p);
          if (len >= sizeof(line)) {
            return -1;
          }
          memcpy(line, p, len);
          line[len] = '\0';
          trim_line(line);
          p = line_end ? line_end + 1 : p + len;
          if (line[0] == '[') {
            break;
          }
          if (line[0] == '\0' || line[0] == '#') {
            continue;
          }
          const char* eq = strchr(line, '=');
          if (eq == NULL) {
            continue;
          }
          char key[64];
          size_t klen = (size_t)(eq - line);
          if (klen >= sizeof(key)) {
            return -1;
          }
          memcpy(key, line, klen);
          key[klen] = '\0';
          trim_line(key);
          eq++;
          while (*eq && isspace((unsigned char)*eq)) {
            eq++;
          }
          if (strcmp(key, "dev") == 0) {
            if (strcmp(eq, "true") == 0 || strcmp(eq, "1") == 0) {
              dev = 1;
            }
          }
        }
      }
    }
  }

  if (mode == 0) {
    g_tls_enabled = 0;
    g_tls_mode = 0;
    g_tls_le_domain_count = 0;
    g_tls_renew_before_days = renew_days;
    g_tls_self_signed_dev = 0;
    g_tls_le_email[0] = '\0';
    return 0;
  }

  g_tls_enabled = 1;
  g_tls_mode = mode;
  g_tls_le_domain_count = domain_count;
  if (g_tls_le_domain_count == 0 && strstr(text, "host") != NULL) {
    g_tls_le_domain_count = 1;
  }
  g_tls_renew_before_days = renew_days > 0 ? renew_days : 30;
  g_tls_self_signed_dev = dev;
  strncpy(g_tls_le_email, email, sizeof(g_tls_le_email) - 1);
  g_tls_le_email[sizeof(g_tls_le_email) - 1] = '\0';
  return 0;
}

static int parse_limits_rate_limit_rps(const char* text, int* out_rps) {
  *out_rps = 0;
  const char* sec = strstr(text, "[limits]");
  if (sec == NULL) {
    return 0;
  }
  char line[512];
  const char* p = strchr(sec, '\n');
  if (p == NULL) {
    return 0;
  }
  while (*p) {
    const char* line_end = strchr(p, '\n');
    size_t len = line_end ? (size_t)(line_end - p) : strlen(p);
    if (len >= sizeof(line)) {
      return -1;
    }
    memcpy(line, p, len);
    line[len] = '\0';
    trim_line(line);
    p = line_end ? line_end + 1 : p + len;
    if (line[0] == '[') {
      break;
    }
    if (line[0] == '\0' || line[0] == '#') {
      continue;
    }
    if (strncmp(line, "rate_limit_rps", 14) == 0) {
      const char* eq = strchr(line, '=');
      if (eq == NULL) {
        continue;
      }
      eq++;
      while (*eq && isspace((unsigned char)*eq)) {
        eq++;
      }
      int n = atoi(eq);
      if (n > 0) {
        *out_rps = n;
      }
      return 0;
    }
  }
  return 0;
}

static int validate_proxy_rate_limits(const char* text) {
  int has_proxy = 0;
  for (int32_t i = 0; i < g_route_count; i++) {
    if (strncmp(g_routes[i].action, "proxy:", 6) == 0) {
      has_proxy = 1;
      break;
    }
  }
  if (!has_proxy) {
    return 0;
  }
  int rps = 0;
  if (parse_limits_rate_limit_rps(text, &rps) != 0) {
    httpd_set_error(5, "invalid [limits] table");
    return -1;
  }
  if (rps <= 0) {
    httpd_set_error(
        5, "limits.rate_limit_rps is required when routes include proxy: (M1 public/agent gate)");
    return -1;
  }
  return 0;
}

static int parse_quoted(const char** pp, char* out, size_t out_len) {
  const char* p = *pp;
  if (*p != '"') {
    return -1;
  }
  p++;
  size_t n = 0;
  while (*p && *p != '"') {
    if (n + 1 >= out_len) {
      return -1;
    }
    out[n++] = *p++;
  }
  if (*p != '"') {
    return -1;
  }
  p++;
  out[n] = '\0';
  *pp = p;
  return 0;
}

static int parse_routes_table(const char* text) {
  const char* sec = strstr(text, "[routes]");
  if (sec == NULL) {
    return 0;
  }
  char line[1024];
  const char* p = strchr(sec, '\n');
  int32_t priority = 0;
  httpd_clear_routes();
  if (p == NULL) {
    return 0;
  }
  while (*p) {
    const char* line_end = strchr(p, '\n');
    size_t len = line_end ? (size_t)(line_end - p) : strlen(p);
    if (len >= sizeof(line)) {
      return -1;
    }
    memcpy(line, p, len);
    line[len] = '\0';
    trim_line(line);
    p = line_end ? line_end + 1 : p + len;
    if (line[0] == '\0' || line[0] == '#') {
      continue;
    }
    if (line[0] == '[') {
      break;
    }
    const char* q = line;
    char key[512];
    char action[256];
    if (parse_quoted(&q, key, sizeof(key)) != 0) {
      httpd_set_error(2, "invalid route key: expected quoted \"METHOD /path\"");
      return -1;
    }
    while (*q && isspace((unsigned char)*q)) {
      q++;
    }
    if (*q != '=') {
      return -1;
    }
    q++;
    while (*q && isspace((unsigned char)*q)) {
      q++;
    }
    if (parse_quoted(&q, action, sizeof(action)) != 0) {
      return -1;
    }
    if (g_route_count >= LI_HTTPD_MAX_ROUTES) {
      return -1;
    }
    if (parse_route_key_value(key, action, priority, &g_routes[g_route_count]) != 0) {
      return -1;
    }
    g_route_count++;
    priority++;
  }
  for (int32_t i = 0; i < g_route_count; i++) {
    g_routes[i].route_id = i + 1;
  }
  return validate_routes();
}

static void normalize_request_path(const char* path, char* out, size_t out_len) {
  if (path == NULL || path[0] == '\0') {
    strncpy(out, "/", out_len - 1);
    out[out_len - 1] = '\0';
    return;
  }
  char tmp[LI_HTTPD_PATH_MAX];
  size_t n = 0;
  const char* p = path;
  if (*p == '/') {
    tmp[n++] = '/';
    p++;
  } else {
    tmp[n++] = '/';
  }
  while (*p && n + 2 < sizeof(tmp)) {
    if (*p == '/') {
      p++;
      continue;
    }
    const char* seg = p;
    while (*p && *p != '/') {
      p++;
    }
    if (n > 1) {
      tmp[n++] = '/';
    }
    size_t seglen = (size_t)(p - seg);
    if (seglen > 0 && n + seglen < sizeof(tmp)) {
      memcpy(tmp + n, seg, seglen);
      n += seglen;
    }
  }
  if (n == 0) {
    tmp[n++] = '/';
  }
  tmp[n] = '\0';
  strncpy(out, tmp, out_len - 1);
  out[out_len - 1] = '\0';
}

static int route_matches(const LiHttpdRoute* route, const char* method, const char* norm_path) {
  if (strcmp(route->method, method) != 0) {
    return 0;
  }
  if (route->path_kind == LI_PATH_EXACT) {
    return li_rt_path_exact(norm_path, route->path);
  }
  return li_rt_path_prefix(norm_path, route->path);
}

int32_t li_rt_httpd_load_config(const char* path) {
  httpd_set_error(0, "");
  if (path == NULL) {
    httpd_set_error(1, "config path is null");
    return -1;
  }
  FILE* f = fopen(path, "rb");
  if (f == NULL) {
    httpd_set_error(1, "cannot open config file");
    return -1;
  }
  char* buf = (char*)malloc(LI_HTTPD_FILE_MAX);
  if (buf == NULL) {
    fclose(f);
    return -1;
  }
  size_t n = fread(buf, 1, LI_HTTPD_FILE_MAX - 1, f);
  fclose(f);
  if (n == 0) {
    free(buf);
    return -1;
  }
  buf[n] = '\0';
  g_m15_stream_idle_sec = 0;
  g_m15_stream_max_sec = 0;
  g_m15_concurrent_streams = 0;
  g_leak_censor_enabled = 0;
  g_leak_censor_deny_path_count = 0;
  g_leak_censor_pattern_openai = 0;
  g_leak_censor_pattern_jwt = 0;
  g_leak_censor_pattern_pem = 0;
  g_tls_enabled = 0;
  g_tls_mode = 0;
  g_tls_le_domain_count = 0;
  g_tls_renew_before_days = 30;
  g_tls_self_signed_dev = 0;
  g_tls_le_email[0] = '\0';
  int rc = parse_m3_config(buf);
  if (rc == 0) {
    rc = parse_m2_config(buf);
  }
  if (rc == 0) {
    rc = parse_limits_m15_stream(buf);
  }
  if (rc == 0) {
    rc = parse_leak_censor_table(buf);
  }
  if (rc == 0) {
    rc = parse_server_tls(buf);
  }
  if (rc == 0) {
    rc = parse_routes_table(buf);
  }
  if (rc == 0) {
    rc = validate_proxy_rate_limits(buf);
  }
  free(buf);
  if (rc != 0 && g_httpd_last_kind == 0) {
    httpd_set_error(5, "invalid [routes] table");
  }
  return rc == 0 ? 0 : -1;
}

int32_t li_rt_httpd_route_key_valid(const char* key) {
  LiHttpdRoute tmp;
  if (key == NULL) {
    return 0;
  }
  if (parse_route_key_value(key, "static:noop", 0, &tmp) != 0) {
    return 0;
  }
  return 1;
}

static int parse_http_request_line(const char* req, char* method, size_t method_len, char* path,
                                   size_t path_len) {
  if (req == NULL || method == NULL || path == NULL) {
    return -1;
  }
  const char* sp1 = strchr(req, ' ');
  if (sp1 == NULL) {
    return -1;
  }
  size_t mlen = (size_t)(sp1 - req);
  if (mlen == 0 || mlen >= method_len) {
    return -1;
  }
  memcpy(method, req, mlen);
  method[mlen] = '\0';
  const char* p = sp1 + 1;
  while (*p == ' ') {
    p++;
  }
  const char* sp2 = strchr(p, ' ');
  if (sp2 == NULL) {
    return -1;
  }
  size_t plen = (size_t)(sp2 - p);
  if (plen == 0 || plen >= path_len) {
    return -1;
  }
  memcpy(path, p, plen);
  path[plen] = '\0';
  return 0;
}

static int32_t httpd_send_response(int32_t conn_fd, int32_t status, const char* body) {
  char hdr[256];
  const char* reason = "OK";
  if (status == 404) {
    reason = "Not Found";
  } else if (status == 502) {
    reason = "Bad Gateway";
  }
  const int blen = body ? (int)strlen(body) : 0;
  snprintf(hdr, sizeof(hdr),
           "HTTP/1.1 %d %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", status, reason,
           blen, body ? body : "");
  tcp_send(conn_fd, hdr);
  return 0;
}

/* M1 oracle: one accept, built-in GET /health, match loaded [routes]. Not the production loop
 * (see draft PRs httpd-m1-impl / httpd-m1-perf for epoll + full serve). */
int32_t li_rt_httpd_serve_routed_once(int32_t port) {
  if (port <= 0 || port > 65535) {
    return -1;
  }
  if (g_route_count == 0) {
    if (li_rt_httpd_load_routing_fixture() != 0) {
      return -1;
    }
  }
  const char* req = "GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n";
  char method[LI_HTTPD_METHOD_MAX];
  char path[LI_HTTPD_PATH_MAX];
  if (parse_http_request_line(req, method, sizeof(method), path, sizeof(path)) != 0) {
    return -1;
  }
  const int32_t route_id = li_rt_httpd_match_route(method, path);
  const int32_t listen_fd = tcp_listen(port);
  const int32_t conn_fd = tcp_accept(listen_fd);
  if (route_id == 0) {
    httpd_send_response(conn_fd, 404, "not found\n");
    tcp_close(conn_fd);
    tcp_close(listen_fd);
    return 1;
  }
  const int32_t kind = li_rt_httpd_route_action_kind(route_id);
  if (kind == 1) {
    httpd_send_response(conn_fd, 200, "ok\n");
    tcp_close(conn_fd);
    tcp_close(listen_fd);
    return 0;
  }
  if (kind == 2) {
    httpd_send_response(conn_fd, 502, "proxy not wired\n");
    tcp_close(conn_fd);
    tcp_close(listen_fd);
    return 2;
  }
  httpd_send_response(conn_fd, 404, "not found\n");
  tcp_close(conn_fd);
  tcp_close(listen_fd);
  return 1;
}

int32_t li_rt_httpd_serve_once(int32_t port) {
  if (port <= 0 || port > 65535) {
    return -1;
  }
  const int32_t listen_fd = tcp_listen(port);
  const int32_t conn_fd = tcp_accept(listen_fd);
  const char* resp = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok";
  tcp_send(conn_fd, resp);
  tcp_close(conn_fd);
  tcp_close(listen_fd);
  return 0;
}

int32_t li_rt_httpd_match_route(const char* method, const char* path) {
  if (method == NULL || path == NULL || g_route_count == 0) {
    return 0;
  }
  char norm[LI_HTTPD_PATH_MAX];
  normalize_request_path(path, norm, sizeof(norm));
  for (int32_t pri = 0; pri < g_route_count; pri++) {
    for (int32_t i = 0; i < g_route_count; i++) {
      if (g_routes[i].priority != pri) {
        continue;
      }
      if (route_matches(&g_routes[i], method, norm)) {
        return g_routes[i].route_id;
      }
    }
  }
  return 0;
}

static const char* path_kind_name(LiPathKind kind) {
  switch (kind) {
    case LI_PATH_PREFIX:
      return "prefix";
    case LI_PATH_PREFIX_STRIP:
      return "prefix_strip";
    case LI_PATH_EXACT:
    default:
      return "exact";
  }
}

int32_t li_rt_httpd_explain_config(const char* path) {
  if (li_rt_httpd_load_config(path) != 0) {
    return -1;
  }
  fputs("# canonical routes (desugared)\n", stdout);
  for (int32_t i = 0; i < g_route_count; i++) {
    const LiHttpdRoute* r = &g_routes[i];
    if (i > 0) {
      fputc('\n', stdout);
    }
    fputs("[[routes]]\n", stdout);
    fprintf(stdout, "name = \"%s\"\n", r->name);
    fprintf(stdout, "priority = %d\n", (int)r->priority);
    fprintf(stdout, "method = \"%s\"\n", r->method);
    fprintf(stdout, "path = \"%s\"\n", r->path);
    fprintf(stdout, "path_kind = \"%s\"\n", path_kind_name(r->path_kind));
    if (r->headers[0] != '\0' || r->require_traceparent || r->require_websocket) {
      char extras[LI_HTTPD_HEADERS_MAX + 64];
      extras[0] = '\0';
      if (r->require_traceparent) {
        strcat(extras, "require=traceparent");
      }
      if (r->require_websocket) {
        if (extras[0] != '\0') {
          strcat(extras, " ");
        }
        strcat(extras, "require=websocket");
      }
      if (r->headers[0] != '\0') {
        if (extras[0] != '\0') {
          strcat(extras, " ");
        }
        strcat(extras, r->headers);
      }
      fprintf(stdout, "action = \"%s\" [%s]\n", r->action, extras);
    } else {
      fprintf(stdout, "action = \"%s\"\n", r->action);
    }
  }
  if (g_route_count > 0) {
    fputc('\n', stdout);
  }
  return 0;
}

static int32_t httpd_load_config_under_repo(const char* rel) {
  const char* root = getenv("LI_REPO_ROOT");
  if (root == NULL || root[0] == '\0' || rel == NULL || rel[0] == '\0') {
    return -1;
  }
  char path[4096];
  int n = snprintf(path, sizeof(path), "%s/%s", root, rel);
  if (n < 0 || (size_t)n >= sizeof(path)) {
    return -1;
  }
  return li_rt_httpd_load_config(path);
}

int32_t li_rt_httpd_load_routing_fixture(void) {
  return httpd_load_config_under_repo("li-tests/httpd/fixtures/routing.toml");
}

int32_t li_rt_httpd_load_m15_agent_fixture(void) {
  return httpd_load_config_under_repo("li-tests/config_desugar/good/agent_m15.toml");
}

int32_t li_rt_httpd_load_m15_leak_censor_fixture(void) {
  return httpd_load_config_under_repo("li-tests/config_desugar/good/leak_censor_m15.toml");
}

int32_t li_rt_httpd_load_m15_tls_le_fixture(void) {
  return httpd_load_config_under_repo("li-tests/config_desugar/good/tls_lets_encrypt_staging.toml");
}

int32_t li_rt_httpd_load_m15_tls_dev_fixture(void) {
  return httpd_load_config_under_repo("li-tests/config_desugar/good/tls_self_signed_dev.toml");
}

int32_t li_rt_httpd_route_action_kind(int32_t route_id) {
  if (route_id <= 0 || route_id > g_route_count) {
    return 0;
  }
  const char* action = g_routes[route_id - 1].action;
  if (strncmp(action, "static:", 7) == 0) {
    return 1;
  }
  if (strncmp(action, "proxy:", 6) == 0) {
    return 2;
  }
  return 0;
}

int32_t li_rt_httpd_parse_duration_sec(const char* raw) {
  int sec = 0;
  if (parse_duration_sec_value(raw, &sec) != 0) {
    return -1;
  }
  return (int32_t)sec;
}

int32_t li_rt_httpd_m15_stream_idle_sec(void) { return g_m15_stream_idle_sec; }

int32_t li_rt_httpd_m15_stream_max_sec(void) { return g_m15_stream_max_sec; }

int32_t li_rt_httpd_m15_concurrent_streams(void) { return g_m15_concurrent_streams; }

int32_t li_rt_httpd_route_requires_traceparent(int32_t route_id) {
  if (route_id <= 0 || route_id > g_route_count) {
    return 0;
  }
  return g_routes[route_id - 1].require_traceparent;
}

int32_t li_rt_httpd_is_sse_content_type(const char* ctype) {
  if (ctype == NULL) {
    return 0;
  }
  return strstr(ctype, "text/event-stream") != NULL ? 1 : 0;
}

static int header_has_traceparent_c(const char* buf, int hdr_end) {
  for (int i = 0; i + 14 < hdr_end; i++) {
    if ((buf[i] == 't' || buf[i] == 'T') && (buf[i + 1] == 'r' || buf[i + 1] == 'R') &&
        strncmp(buf + i, "traceparent:", 12) == 0) {
      return 1;
    }
  }
  return 0;
}

int32_t li_rt_httpd_traceparent_ok(const char* buf, int32_t hdr_end) {
  if (buf == NULL || hdr_end <= 0) {
    return 0;
  }
  return header_has_traceparent_c(buf, hdr_end) ? 1 : 0;
}

int32_t li_rt_httpd_traceparent_selftest(void) {
  static const char hdr[] =
      "POST /x HTTP/1.1\r\n"
      "traceparent: 00-abc-def-01\r\n"
      "\r\n";
  const int hdr_end = (int)(sizeof(hdr) - 1);
  return header_has_traceparent_c(hdr, hdr_end) ? 0 : -1;
}

int32_t li_rt_httpd_leak_censor_enabled(void) { return g_leak_censor_enabled; }

int32_t li_rt_httpd_leak_censor_deny_path_count(void) { return g_leak_censor_deny_path_count; }

int32_t li_rt_httpd_leak_censor_pattern_openai(void) { return g_leak_censor_pattern_openai; }

int32_t li_rt_httpd_leak_censor_pattern_jwt(void) { return g_leak_censor_pattern_jwt; }

int32_t li_rt_httpd_leak_censor_pattern_pem(void) { return g_leak_censor_pattern_pem; }

int32_t li_rt_httpd_tls_enabled(void) { return g_tls_enabled; }

int32_t li_rt_httpd_tls_mode(void) { return g_tls_mode; }

int32_t li_rt_httpd_tls_le_domain_count(void) { return g_tls_le_domain_count; }

int32_t li_rt_httpd_tls_renew_before_days(void) { return g_tls_renew_before_days; }

int32_t li_rt_httpd_tls_self_signed_dev(void) { return g_tls_self_signed_dev; }

const char* li_rt_httpd_tls_le_email(void) { return g_tls_le_email; }

int32_t li_rt_httpd_tls_selftest(void) {
  if (g_tls_enabled == 0) {
    return -1;
  }
  if (g_tls_mode == 3 && g_tls_le_email[0] == '\0') {
    return -2;
  }
  if (g_tls_mode == 3 && g_tls_le_domain_count < 1) {
    return -3;
  }
  return 0;
}

int32_t li_rt_httpd_m2_enabled(void) { return g_m2_enabled; }

int32_t li_rt_httpd_m2_tls_terminate(void) { return g_m2_tls_terminate; }

int32_t li_rt_httpd_m2_http2_enabled(void) { return g_m2_http2_enabled; }

int32_t li_rt_httpd_m2_http2_max_streams(void) { return g_m2_http2_max_streams; }

int32_t li_rt_httpd_m2_queue_max_depth(void) { return g_m2_queue_max_depth; }

int32_t li_rt_httpd_m2_queue_retry_after_sec(void) { return g_m2_queue_retry_after_sec; }

int32_t li_rt_httpd_m2_cb_error_threshold(void) { return g_m2_cb_error_threshold; }

int32_t li_rt_httpd_m2_webhook_allow_count(void) { return g_m2_webhook_allow_count; }

int32_t li_rt_httpd_route_requires_websocket(int32_t route_id) {
  if (route_id <= 0 || route_id > g_route_count) {
    return 0;
  }
  return g_routes[route_id - 1].require_websocket;
}

int32_t li_rt_httpd_m2_webhook_url_allowed(const char* url) {
  if (url == NULL || g_m2_webhook_allow_count == 0) {
    return 0;
  }
  for (int i = 0; i < g_m2_webhook_allow_count; i++) {
    if (strcmp(url, g_m2_webhook_allow[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

int32_t li_rt_httpd_m2_selftest(void) {
  if (g_m2_enabled == 0) {
    return -1;
  }
  if (g_m2_http2_enabled && !g_m2_tls_terminate) {
    return -2;
  }
  if (g_m2_tls_terminate && g_tls_enabled == 0) {
    return -3;
  }
  if (g_m2_queue_max_depth > 0 && g_m2_queue_retry_after_sec < 1) {
    return -4;
  }
  if (g_m2_cb_error_threshold > 0 && g_m2_cb_window_sec < 1) {
    return -5;
  }
  if (g_m2_webhook_allow_count > 0 && !li_rt_httpd_m2_webhook_url_allowed(g_m2_webhook_allow[0])) {
    return -6;
  }
  return 0;
}

int32_t li_rt_httpd_m3_enabled(void) { return g_m3_enabled; }

int32_t li_rt_httpd_m3_l4_enabled(void) { return g_m3_l4_enabled; }

int32_t li_rt_httpd_m3_l4_listen_port(void) { return g_m3_l4_listen_port; }

int32_t li_rt_httpd_m3_l4_upstream_port(void) { return g_m3_l4_upstream_port; }

int32_t li_rt_httpd_m3_l4_max_connections(void) { return g_m3_l4_max_connections; }

int32_t li_rt_httpd_m3_token_budget_enabled(void) { return g_m3_token_budget_enabled; }

int32_t li_rt_httpd_m3_token_budget_max(void) { return g_m3_token_budget_max; }

static int m3_parse_nonneg_int(const char* s, int64_t* out) {
  if (s == NULL || !s[0]) {
    return -1;
  }
  int64_t v = 0;
  for (const char* p = s; *p; p++) {
    if (*p < '0' || *p > '9') {
      return -1;
    }
    v = v * 10 + (*p - '0');
    if (v > 10000000000LL) {
      return -1;
    }
  }
  *out = v;
  return 0;
}

int32_t li_rt_httpd_m3_token_budget_check(const char* header_val) {
  if (!g_m3_token_budget_enabled || header_val == NULL) {
    return 1;
  }
  int64_t v = 0;
  if (m3_parse_nonneg_int(header_val, &v) != 0) {
    return -2;
  }
  if (v == 0) {
    return 0;
  }
  if (g_m3_token_budget_reject_over && v > (int64_t)g_m3_token_budget_max) {
    return 0;
  }
  return 1;
}

int32_t li_rt_httpd_m3_selftest(void) {
  if (g_m3_enabled == 0) {
    return -1;
  }
  if (g_m3_l4_enabled) {
    if (g_m3_l4_listen_port <= 0 || g_m3_l4_upstream_port <= 0 || g_m3_l4_upstream_host[0] == '\0') {
      return -2;
    }
    if (g_m3_l4_max_connections <= 0) {
      return -3;
    }
  }
  if (g_m3_token_budget_enabled) {
    if (g_m3_token_budget_max <= 0) {
      return -4;
    }
    if (li_rt_httpd_m3_token_budget_check("1") != 1) {
      return -5;
    }
  }
  return 0;
}
