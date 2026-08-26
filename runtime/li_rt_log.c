/* M1 li-log runtime seam — access/error file sinks, redaction, size rotation. */

#include "li_rt.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define LI_LOG_DIR_MAX 512
#define LI_LOG_PATH_MAX 640
#define LI_LOG_LINE_MAX 4096
#define LI_LOG_REDACT_MAX 4096
/* 100 MiB — matches httpd plan default max_size */
#define LI_LOG_ROTATE_BYTES (100LL * 1024LL * 1024LL)

static char g_log_dir[LI_LOG_DIR_MAX] = "./logs";
static FILE* g_access_fp = NULL;
static char g_access_path[LI_LOG_PATH_MAX];

static int li_log_ensure_dir(void) {
  struct stat st;
  if (stat(g_log_dir, &st) == 0) {
    if (S_ISDIR(st.st_mode)) {
      return 0;
    }
    return -1;
  }
  if (mkdir(g_log_dir, 0750) != 0 && errno != EEXIST) {
    return -1;
  }
  return 0;
}

static void li_log_close_access(void) {
  if (g_access_fp != NULL) {
    fclose(g_access_fp);
    g_access_fp = NULL;
  }
}

static int li_log_rotate_if_needed(const char* path) {
  struct stat st;
  if (stat(path, &st) != 0) {
    return 0;
  }
  if (st.st_size < LI_LOG_ROTATE_BYTES) {
    return 0;
  }
  char backup[LI_LOG_PATH_MAX];
  snprintf(backup, sizeof(backup), "%s.1", path);
  remove(backup);
  if (rename(path, backup) != 0) {
    return -1;
  }
  return 0;
}

static int li_log_open_access(void) {
  if (g_access_fp != NULL) {
    return 0;
  }
  if (li_log_ensure_dir() != 0) {
    return -1;
  }
  snprintf(g_access_path, sizeof(g_access_path), "%s/access.log", g_log_dir);
  li_log_rotate_if_needed(g_access_path);
  g_access_fp = fopen(g_access_path, "a");
  if (g_access_fp == NULL) {
    return -1;
  }
  return 0;
}

void li_rt_log_set_dir(const char* dir) {
  if (dir == NULL || dir[0] == '\0') {
    strncpy(g_log_dir, "./logs", sizeof(g_log_dir) - 1);
    g_log_dir[sizeof(g_log_dir) - 1] = '\0';
  } else {
    strncpy(g_log_dir, dir, sizeof(g_log_dir) - 1);
    g_log_dir[sizeof(g_log_dir) - 1] = '\0';
  }
  li_log_close_access();
}

static int li_log_ci_match(const char* hay, const char* needle) {
  size_t n = strlen(needle);
  for (const char* p = hay; *p; ++p) {
    size_t i = 0;
    while (i < n && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) {
      ++i;
    }
    if (i == n) {
      return 1;
    }
  }
  return 0;
}

static void li_log_redact_bearer(char* s) {
  const char* markers[] = {"Bearer ", "bearer ", NULL};
  for (int m = 0; markers[m]; ++m) {
    char* p = s;
    while ((p = strstr(p, markers[m])) != NULL) {
      char* t = p + strlen(markers[m]);
      while (*t && *t != '\r' && *t != '\n' && *t != ' ' && *t != '\t') {
        *t = '*';
        ++t;
      }
      p = t;
    }
  }
}

static void li_log_redact_header_value(char* s, const char* header) {
  char pat[64];
  snprintf(pat, sizeof(pat), "%s:", header);
  char* p = s;
  while ((p = strstr(p, pat)) != NULL) {
    char* v = p + strlen(pat);
    while (*v == ' ' || *v == '\t') {
      ++v;
    }
    while (*v && *v != '\r' && *v != '\n') {
      *v = '*';
      ++v;
    }
    p = v;
  }
}

static void li_log_redact_sk(char* s) {
  char* p = s;
  while ((p = strstr(p, "sk-")) != NULL) {
    char* t = p + 3;
    int n = 0;
    while (*t && (isalnum((unsigned char)*t) || *t == '_' || *t == '-')) {
      *t = '*';
      ++t;
      ++n;
      if (n > 48) {
        break;
      }
    }
    p = t;
  }
}

int32_t li_rt_log_redact_line(const char* in, char* out, int32_t cap) {
  if (in == NULL || out == NULL || cap <= 1) {
    return 0;
  }
  size_t len = strlen(in);
  if (len >= (size_t)cap) {
    len = (size_t)cap - 1;
  }
  memcpy(out, in, len);
  out[len] = '\0';
  li_log_redact_bearer(out);
  li_log_redact_header_value(out, "authorization");
  li_log_redact_header_value(out, "cookie");
  li_log_redact_header_value(out, "x-api-key");
  li_log_redact_sk(out);
  return (int32_t)strlen(out);
}

void li_rt_log_access_line(const char* ts, const char* method, const char* path, int32_t status,
                           int32_t bytes_out) {
  char line[LI_LOG_LINE_MAX];
  char rpath[256];
  char rmethod[32];
  if (path == NULL) {
    snprintf(rpath, sizeof(rpath), "/");
  } else {
    strncpy(rpath, path, sizeof(rpath) - 1);
    rpath[sizeof(rpath) - 1] = '\0';
  }
  if (method == NULL) {
    snprintf(rmethod, sizeof(rmethod), "GET");
  } else {
    strncpy(rmethod, method, sizeof(rmethod) - 1);
    rmethod[sizeof(rmethod) - 1] = '\0';
  }
  li_rt_log_redact_line(rpath, rpath, (int32_t)sizeof(rpath));
  li_rt_log_redact_line(rmethod, rmethod, (int32_t)sizeof(rmethod));
  const char* t = (ts != NULL && ts[0]) ? ts : "1970-01-01T00:00:00Z";
  snprintf(line, sizeof(line), "%s info - %s %s %d %d\n", t, rmethod, rpath, (int)status,
           (int)bytes_out);
  li_rt_log_redact_line(line, line, (int32_t)sizeof(line));

  if (li_log_open_access() != 0) {
    fputs(line, stderr);
    return;
  }
  fputs(line, g_access_fp);
  fflush(g_access_fp);
}

int32_t li_rt_log_reopen(void) {
  li_log_close_access();
  return li_log_open_access() == 0 ? 0 : -1;
}

const char* li_rt_log_redact(const char* in) {
  static char g_redact_buf[LI_LOG_REDACT_MAX];
  if (in == NULL) {
    g_redact_buf[0] = '\0';
    return g_redact_buf;
  }
  li_rt_log_redact_line(in, g_redact_buf, (int32_t)sizeof(g_redact_buf));
  return g_redact_buf;
}

/* Test helper: 0 if redacted line contains no raw secret substrings. */
int32_t li_rt_log_redact_ok(const char* in) {
  const char* out = li_rt_log_redact(in);
  if (out == NULL) {
    return -1;
  }
  if (strstr(out, "supersecret") != NULL) {
    return -1;
  }
  if (strstr(out, "sk-live") != NULL) {
    return -1;
  }
  if (li_log_ci_match(out, "Bearer supersecret")) {
    return -1;
  }
  return 0;
}
