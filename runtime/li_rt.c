#include "li_rt.h"
#include "li_parallel.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <pthread.h>
#include <unistd.h>
#endif

void li_panic(const char* msg) {
  fprintf(stderr, "li panic: %s\n", msg);
  abort();
}

void li_bounds_fail(void) { li_panic("array index out of bounds"); }

void li_rt_print_int(int32_t value) { printf("%d\n", value); }

void li_rt_print_str(const char* s) { puts(s); }

/* Self-hosted lexer seam: emit one token as `kind<TAB>lexeme<NL>`, where the
 * lexeme is the byte range text[start, end). Matches the C++ `lic lex` dump. */
int32_t li_rt_emit_token(int32_t kind, const char* text, int32_t start, int32_t end) {
  printf("%d\t", kind);
  if (text != NULL && start >= 0 && end > start) {
    fwrite(text + start, 1, (size_t)(end - start), stdout);
  }
  fputc('\n', stdout);
  return 0;
}

/* Self-hosted AST dump primitives: stream one node line as `code field...`.
 * The li bootstrap parser calls these in parse order; the C++ `lic ast`
 * subcommand prints the same bytes from the C++ AST (check_li_ast_parity.sh). */
int32_t li_rt_ast_int(int32_t v) {
  printf("%d", (int)v);
  return 0;
}

int32_t li_rt_ast_text(const char* text, int32_t start, int32_t end) {
  if (text != NULL && start >= 0 && end > start) {
    fwrite(text + start, 1, (size_t)(end - start), stdout);
  }
  return 0;
}

int32_t li_rt_ast_space(void) {
  fputc(' ', stdout);
  return 0;
}

/* Ring of static buffers for try_read_file so multiple imports don't
 * overwrite each other.  Each slot is 1 MiB; the ring head advances on
 * every successful read.  li_rt_resolve_import needs at least ~30 stable
 * buffers for transitive imports (std + workspace + same-dir). */
#define TRY_FILE_RING 64
#define TRY_FILE_BUFSZ (1 << 20)
static char  try_file_ring[TRY_FILE_RING][TRY_FILE_BUFSZ];
static int   try_file_ring_head = 0;

/* Helper: try to read the file at `path`, returning its content in the next
 * ring slot or NULL. */
static const char* try_read_file(const char* path) {
  FILE* f = fopen(path, "rb");
  if (f == NULL) {
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  const long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* content = try_file_ring[try_file_ring_head];
  if (sz < 0 || (size_t)sz >= TRY_FILE_BUFSZ) {
    fclose(f);
    return NULL;
  }
  const size_t got = fread(content, 1, (size_t)sz, f);
  fclose(f);
  content[got] = '\0';
  try_file_ring_head = (try_file_ring_head + 1) % TRY_FILE_RING;
  return content;
}

/* Walk up from the file to find the workspace root (directory containing
 * a `packages/` subdirectory). Populates `root` (up to root_cap bytes) and
 * returns the length, or 0 on failure. */
static size_t find_workspace_root(const char* file_path, char* root,
                                  size_t root_cap) {
  const char* slash = strrchr(file_path, '/');
  if (slash == NULL) {
    return 0;
  }
  size_t len = (size_t)(slash - file_path);
  while (len > 0) {
    if (len + 10 >= root_cap) {
      return 0;
    }
    memcpy(root, file_path, len);
    root[len] = '\0';
    strcat(root, "/packages");
    DIR* d = opendir(root);
    root[len] = '\0';  /* undo strcat */
    if (d != NULL) {
      closedir(d);
      return len;
    }
    /* Walk up one level */
    while (len > 0 && root[len - 1] != '/') {
      --len;
    }
    if (len > 0) {
      --len;  /* skip the '/' */
    }
  }
  /* Fallback: check current directory */
  {
    DIR* d = opendir("packages");
    if (d != NULL) {
      closedir(d);
      root[0] = '.';
      root[1] = '\0';
      return 1;
    }
  }
  return 0;
}

/* Resolve a single-segment import (e.g. `vault_lib`) or a dotted workspace
 * import (e.g. `physics.rigid`) to a file path.
 *
 * Try candidates in order:
 * 1. Same-directory sibling: <dir-of-file>/<module>.li
 * 2. Workspace package:    <workspace-root>/packages/li-<mod-dashed>/src/lib.li
 *
 * Returns the file contents, or NULL when no candidate exists. */
/* Indexed import path store: lets the Li walker retrieve the resolved
 * file path for each import by index, so transitive resolution uses the
 * correct base directory. */
/* Global proc-count accumulator for the Li MIR walker.  Works around a
 * Li compiler bug where var array parameters are passed by value in
 * some code paths, causing pn[0] to not propagate across nested
 * mir_walk calls (e.g. the import collect loop). */
static int32_t g_li_pn = 0;
int32_t li_rt_pn_get(void) { return g_li_pn; }
void li_rt_pn_set(int32_t v) { g_li_pn = v; }

/* Global param-index accumulator — separate from pn because
 * mir_collect_proc uses this as the param-slot index into ptok2/pty/etc,
 * while mir_walk uses pn as the proc-id counter. */
static int32_t g_li_pidx = 0;
int32_t li_rt_pidx_get(void) { return g_li_pidx; }
void li_rt_pidx_set(int32_t v) { g_li_pidx = v; }

/* ---- Global param registry (Layer 5 MIR parity) ----
 * Works around the Li compiler bug where var-array parameters are passed
 * by value.  mir_collect_proc writes param metadata here; mir_proc reads
 * it back during emit.  Tables:
 *   0=ptok  1=pex  2=pp0  3=ppn  4=pret  (proc-level, 128 slots)
 *   5=ptok2 6=pty  7=pelems 8=pef 9=pei
 *   10=pvar 11=pmx 12=pmc  (param-slot-level, 512 slots) */
#define LI_RT_PREG_PMAX 512
#define LI_RT_PREG_SMAX 512
#define LI_RT_PREG_TABLES 14
static int32_t g_li_preg[LI_RT_PREG_TABLES * LI_RT_PREG_SMAX];
static int32_t g_li_preg_sizes[LI_RT_PREG_TABLES] = {
  LI_RT_PREG_PMAX, LI_RT_PREG_PMAX, LI_RT_PREG_PMAX, LI_RT_PREG_PMAX, LI_RT_PREG_PMAX,
  LI_RT_PREG_SMAX, LI_RT_PREG_SMAX, LI_RT_PREG_SMAX, LI_RT_PREG_SMAX, LI_RT_PREG_SMAX,
  LI_RT_PREG_SMAX, LI_RT_PREG_SMAX, LI_RT_PREG_SMAX, LI_RT_PREG_SMAX
};

int32_t li_rt_preg_get(int32_t table, int32_t idx) {
  if (table < 0 || table >= LI_RT_PREG_TABLES) return 0;
  if (idx < 0 || idx >= g_li_preg_sizes[table]) return 0;
  return g_li_preg[table * LI_RT_PREG_SMAX + idx];
}

void li_rt_preg_set(int32_t table, int32_t idx, int32_t val) {
  if (table < 0 || table >= LI_RT_PREG_TABLES) return;
  if (idx < 0 || idx >= g_li_preg_sizes[table]) return;
  g_li_preg[table * LI_RT_PREG_SMAX + idx] = val;
}

void li_rt_preg_clear(void) {
  memset(g_li_preg, 0, sizeof(g_li_preg));
}

#define LI_RT_IMPORT_PATH_MAX 32
static const char* li_rt_import_paths[LI_RT_IMPORT_PATH_MAX];
static const char* li_rt_import_contents[LI_RT_IMPORT_PATH_MAX];
static int         li_rt_import_path_count = 0;
static int         li_rt_import_content_count = 0;

void li_rt_import_paths_clear(void) {
  li_rt_import_path_count = 0;
  li_rt_import_content_count = 0;
}

void li_rt_import_content_store(const char* content) {
  if (content != NULL && li_rt_import_content_count < LI_RT_IMPORT_PATH_MAX) {
    li_rt_import_contents[li_rt_import_content_count++] = content;
  }
}

const char* li_rt_import_content_get(int idx) {
  if (idx < 0 || idx >= li_rt_import_content_count) return NULL;
  return li_rt_import_contents[idx];
}

void li_rt_import_content_insert(int idx, const char* content) {
  if (content == NULL || idx < 0 || idx > li_rt_import_content_count ||
      li_rt_import_content_count >= LI_RT_IMPORT_PATH_MAX) return;
  for (int i = li_rt_import_content_count; i > idx; --i) {
    li_rt_import_contents[i] = li_rt_import_contents[i - 1];
  }
  li_rt_import_contents[idx] = content;
  li_rt_import_content_count++;
}

/* Store a resolved path at the next index. Called after each successful
 * li_rt_resolve_import in the Li walker. */
void li_rt_import_path_store(const char* path) {
  if (li_rt_import_path_count < LI_RT_IMPORT_PATH_MAX) {
    /* Duplicate the path since the ring buffer may recycle slots. */
    size_t len = strlen(path);
    char* dup = (char*)malloc(len + 1);
    if (dup) { memcpy(dup, path, len + 1); }
    li_rt_import_paths[li_rt_import_path_count] = dup;
    li_rt_import_path_count++;
  }
}

/* Insert a canonical path at `idx`, preserving the order of the content
 * pointer queue maintained by the self-hosted walker. */
void li_rt_import_path_insert(int idx, const char* path) {
  if (path == NULL || idx < 0 || idx > li_rt_import_path_count ||
      li_rt_import_path_count >= LI_RT_IMPORT_PATH_MAX) return;
  for (int i = li_rt_import_path_count; i > idx; --i) {
    li_rt_import_paths[i] = li_rt_import_paths[i - 1];
  }
  size_t len = strlen(path);
  char* dup = (char*)malloc(len + 1);
  if (dup) { memcpy(dup, path, len + 1); }
  li_rt_import_paths[idx] = dup;
  li_rt_import_path_count++;
}

/* Retrieve the stored path for import index `idx`. Returns NULL if out of range. */
const char* li_rt_import_path_get(int idx) {
  if (idx < 0 || idx >= li_rt_import_path_count) return NULL;
  return li_rt_import_paths[idx];
}

/* Per-type source pointer store: lets field name positions be read from the
 * correct source buffer even when the type was defined in a different file. */
#define LI_RT_TYPE_SRC_MAX 30
static const char* li_rt_type_srcs[LI_RT_TYPE_SRC_MAX];
static const char* li_rt_type_src_fld = NULL;
void li_rt_type_src_store(int idx, const char* src) {
  if (idx == -1) { li_rt_type_src_fld = src; return; }
  if (idx >= 0 && idx < LI_RT_TYPE_SRC_MAX) li_rt_type_srcs[idx] = src;
}
const char* li_rt_type_src_get(int idx) {
  if (idx == -1) return li_rt_type_src_fld;
  if (idx < 0 || idx >= LI_RT_TYPE_SRC_MAX) return NULL;
  return li_rt_type_srcs[idx];
}

/* Path ring buffer: mirrors the content ring buffer so callers can retrieve
 * the path that was successfully resolved by li_rt_resolve_import. */
static char  li_rt_path_ring[TRY_FILE_RING][4096];
static int   li_rt_path_ring_head = 0;

/* Return a pointer to the path that was last successfully resolved by
 * li_rt_resolve_import.  The pointer is stable until TRY_FILE_RING more
 * resolve calls overwrite the ring slot. */
const char* li_rt_resolve_import_last_path(void) {
  if (li_rt_path_ring_head == 0) return li_rt_path_ring[TRY_FILE_RING - 1];
  return li_rt_path_ring[li_rt_path_ring_head - 1];
}

/* Store the resolved path into the path ring and return the file content. */
static const char* store_resolved_path(const char* content, const char* path) {
  size_t len = strlen(path);
  if (len >= 4096) len = 4095;
  memcpy(li_rt_path_ring[li_rt_path_ring_head], path, len);
  li_rt_path_ring[li_rt_path_ring_head][len] = '\0';
  li_rt_path_ring_head = (li_rt_path_ring_head + 1) % TRY_FILE_RING;
  return content;
}

const char* li_rt_resolve_import(const char* file_path, const char* module) {
  if (file_path == NULL || module == NULL) {
    return NULL;
  }
  const char* slash = strrchr(file_path, '/');
  const size_t dir_len = slash != NULL ? (size_t)(slash - file_path) : 0;
  const size_t mlen = strlen(module);
  static char buf[4096];

  /* Candidate 1: same-directory sibling */
  if (dir_len + 1 + mlen + 3 < sizeof(buf)) {
    size_t p = 0;
    if (dir_len > 0) {
      memcpy(buf, file_path, dir_len);
      p = dir_len;
    }
    buf[p++] = '/';
    memcpy(buf + p, module, mlen);
    p += mlen;
    memcpy(buf + p, ".li", 3);
    p += 3;
    buf[p] = '\0';
    const char* result = try_read_file(buf);
    if (result != NULL) {
      return store_resolved_path(result, buf);
    }
  }

  /* Candidate 1b: same-directory package <module>/<module>.li */
  if (dir_len + 1 + mlen + 1 + mlen + 3 + 1 < sizeof(buf)) {
    size_t p = 0;
    if (dir_len > 0) {
      memcpy(buf, file_path, dir_len);
      p = dir_len;
    }
    buf[p++] = '/';
    memcpy(buf + p, module, mlen);
    p += mlen;
    buf[p++] = '/';
    memcpy(buf + p, module, mlen);
    p += mlen;
    memcpy(buf + p, ".li", 3);
    p += 3;
    buf[p] = '\0';
    const char* result2 = try_read_file(buf);
    if (result2 != NULL) {
      return store_resolved_path(result2, buf);
    }
  }

  /* Candidate 2: workspace package */
  char root[4096];
  size_t root_len = find_workspace_root(file_path, root, sizeof(root));
  if (root_len > 0) {
    /* Candidate 2a: packages/li-<module-dashed>/src/lib.li
     * Convert dots and underscores to hyphens so that
     *   import li_demo → packages/li-demo/src/lib.li
     *   import net.httpd → packages/li-net-httpd/src/lib.li */
    if (root_len + 11 + mlen + 12 < sizeof(buf)) {
      size_t p = 0;
      memcpy(buf + p, root, root_len);
      p = root_len;
      memcpy(buf + p, "/packages/li-", 13);
      p += 13;
      for (size_t i = 0; i < mlen && p < sizeof(buf) - 1; ++i) {
        char c = module[i];
        if (c == '.' || c == '_') c = '-';
        buf[p++] = c;
      }
      memcpy(buf + p, "/src/lib.li", 12);
      p += 12;
      buf[p] = '\0';
      const char* result = try_read_file(buf);
      if (result != NULL) {
        return store_resolved_path(result, buf);
      }
    }
    /* Candidate 2b: packages/<module>/src/lib.li (no li- prefix)
     * Handles imports that match a package directory directly. */
    if (root_len + 10 + mlen + 12 < sizeof(buf)) {
      size_t p = 0;
      memcpy(buf + p, root, root_len);
      p = root_len;
      memcpy(buf + p, "/packages/", 10);
      p += 10;
      for (size_t i = 0; i < mlen && p < sizeof(buf) - 1; ++i) {
        char c = module[i];
        if (c == '.') c = '-';
        buf[p++] = c;
      }
      memcpy(buf + p, "/src/lib.li", 12);
      p += 12;
      buf[p] = '\0';
      const char* result2 = try_read_file(buf);
      if (result2 != NULL) {
        return store_resolved_path(result2, buf);
      }
    }
    /* Candidate 2c: strip li- prefix, then dash-convert.
     * import li_demo → packages/li-demo/src/lib.li
     * (C++ workspace_package_entry matches kebab_to_snake(member)) */
    if (mlen > 3 && module[0] == 'l' && module[1] == 'i' && module[2] == '_') {
      const char* stripped = module + 3;
      const size_t slen = mlen - 3;
      if (root_len + 10 + slen + 12 < sizeof(buf)) {
        size_t p = 0;
        memcpy(buf + p, root, root_len);
        p = root_len;
        memcpy(buf + p, "/packages/li-", 13);
        p = root_len + 13;
        for (size_t i = 0; i < slen && p < sizeof(buf) - 1; ++i) {
          char c = stripped[i];
          if (c == '.' || c == '_') c = '-';
          buf[p++] = c;
        }
        memcpy(buf + p, "/src/lib.li", 12);
        p += 12;
        buf[p] = '\0';
        const char* result3 = try_read_file(buf);
        if (result3 != NULL) {
          return store_resolved_path(result3, buf);
        }
      }
    }
    /* Candidate 3: "std.X.Y" → "std/X/Y.li" (stdlib tree) */
    if (mlen > 4 && memcmp(module, "std.", 4) == 0) {
      const char* stripped = module + 4;
      const size_t slen = mlen - 4;
      /* Find last dot for the leaf filename */
      size_t last_dot = 0;
      for (size_t i = 0; i < slen; ++i) {
        if (stripped[i] == '.') last_dot = i;
      }
      /* Build: root/std/<all-but-last-sep>/<last-sep>.li */
      if (root_len + 5 + slen + 3 + 200 < sizeof(buf)) {
        size_t p = 0;
        memcpy(buf + p, root, root_len);
        p = root_len;
        buf[p++] = '/';
        memcpy(buf + p, "std/", 4);
        p += 4;
        /* Copy prefix (before last dot), replacing '.' with '/' */
        for (size_t i = 0; i < last_dot && p < sizeof(buf) - 1; ++i) {
          buf[p++] = stripped[i] == '.' ? '/' : stripped[i];
        }
        if (last_dot > 0) {
          buf[p++] = '/';
        }
        /* Copy last segment as filename */
        for (size_t i = last_dot + 1; i < slen && p < sizeof(buf) - 1; ++i) {
          buf[p++] = stripped[i];
        }
        memcpy(buf + p, ".li", 3);
        p += 3;
        buf[p] = '\0';
        const char* result = try_read_file(buf);
        if (result != NULL) {
          return store_resolved_path(result, buf);
        }
      }
      /* Candidate 3b: std/X/X.li package directory pattern */
      if (root_len + 5 + slen + 1 + slen + 3 + 200 < sizeof(buf)) {
        size_t p = 0;
        memcpy(buf + p, root, root_len);
        p = root_len;
        buf[p++] = '/';
        memcpy(buf + p, "std/", 4);
        p += 4;
        memcpy(buf + p, stripped, slen);
        p += slen;
        buf[p++] = '/';
        memcpy(buf + p, stripped, slen);
        p += slen;
        memcpy(buf + p, ".li", 3);
        p += 3;
        buf[p] = '\0';
        const char* result3b = try_read_file(buf);
        if (result3b != NULL) {
          return store_resolved_path(result3b, buf);
        }
      }
      /* Candidate 4: stripped name as package (fallback) */
      if (root_len + 11 + slen + 12 < sizeof(buf)) {
        size_t p = 0;
        memcpy(buf + p, root, root_len);
        p = root_len;
        memcpy(buf + p, "/packages/li-", 13);
        p += 13;
        for (size_t i = 0; i < slen && p < sizeof(buf) - 1; ++i) {
          buf[p++] = stripped[i] == '.' ? '-' : stripped[i];
        }
        memcpy(buf + p, "/src/lib.li", 12);
        p += 12;
        buf[p] = '\0';
        const char* result = try_read_file(buf);
        if (result != NULL) {
          return store_resolved_path(result, buf);
        }
      }
    }
  }

  return NULL;
}

/* Self-hosted typechecker diagnostic seam: emit one `error [CODE]` line for a
 * numeric error code so the li `check` subcommand can be diffed against the
 * C++ `lic check` diagnostics (scripts/check_li_check_parity.sh). Codes:
 *   201=E0201 202=E0202 301=E0301 302=E0302 303=E0303 304=E0304
 *   310=E0310 311=E0311 320=E0320 321=E0321 322=E0322 330=E0330
 *   any other (900) -> lic.error
 */
int32_t li_rt_emit_err(int32_t code) {
  switch (code) {
    case 201: fputs("module:1:1: error [E0201]\n", stdout); break;
    case 202: fputs("module:1:1: error [E0202]\n", stdout); break;
    case 301: fputs("module:1:1: error [E0301]\n", stdout); break;
    case 302: fputs("module:1:1: error [E0302]\n", stdout); break;
    case 303: fputs("module:1:1: error [E0303]\n", stdout); break;
    case 304: fputs("module:1:1: error [E0304]\n", stdout); break;
    case 310: fputs("module:1:1: error [E0310]\n", stdout); break;
    case 311: fputs("module:1:1: error [E0311]\n", stdout); break;
    case 320: fputs("module:1:1: error [E0320]\n", stdout); break;
    case 321: fputs("module:1:1: error [E0321]\n", stdout); break;
    case 322: fputs("module:1:1: error [E0322]\n", stdout); break;
    case 330: fputs("module:1:1: error [E0330]\n", stdout); break;
    case 401: fputs("module:1:1: warning [W0401]\n", stdout); break;
    default:  fputs("module:1:1: error [lic.error]\n", stdout); break;
  }
  return 0;
}

int32_t li_rt_ast_nl(void) {
  fputc('\n', stdout);
  return 0;
}

/* ---- Self-hosted MIR dump primitives (Layer 5) ---- */

/* Float literals are parsed into a static table; the li bootstrap carries the
 * id (a small int) instead of a double, and formats at emission time. */
static double li_mir_fvals[256];
static int li_mir_fvals_n = 0;

int32_t li_rt_mir_f64(const char* text, int32_t start, int32_t end) {
  if (text == NULL || start < 0 || end <= start) {
    return -1;
  }
  char buf[64];
  size_t n = (size_t)(end - start);
  if (n >= sizeof(buf)) {
    n = sizeof(buf) - 1;
  }
  memcpy(buf, text + start, n);
  buf[n] = '\0';
  const double v = strtod(buf, NULL);
  if (li_mir_fvals_n < (int)(sizeof(li_mir_fvals) / sizeof(li_mir_fvals[0]))) {
    li_mir_fvals[li_mir_fvals_n] = v;
    return li_mir_fvals_n++;
  }
  return -1;
}

int32_t li_rt_mir_f64_fmt(int32_t id) {
  if (id >= 0 && id < li_mir_fvals_n) {
    printf("%.17g", li_mir_fvals[id]);
  } else {
    fputc('0', stdout);
  }
  return 0;
}

int32_t li_rt_mir_f64_of_int(int32_t v) {
  if (li_mir_fvals_n < (int)(sizeof(li_mir_fvals) / sizeof(li_mir_fvals[0]))) {
    li_mir_fvals[li_mir_fvals_n] = (double)v;
    return li_mir_fvals_n++;
  }
  return -1;
}

int32_t li_rt_mir_f64_neg(int32_t id) {
  if (id >= 0 && id < li_mir_fvals_n &&
      li_mir_fvals_n < (int)(sizeof(li_mir_fvals) / sizeof(li_mir_fvals[0]))) {
    li_mir_fvals[li_mir_fvals_n] = -li_mir_fvals[id];
    return li_mir_fvals_n++;
  }
  return -1;
}

int32_t li_rt_mir_int(const char* text, int32_t start, int32_t end) {
  if (text == NULL || start < 0 || end <= start) {
    return 0;
  }
  int64_t v = 0;
  for (int32_t i = start; i < end; ++i) {
    const unsigned char c = (unsigned char)text[i];
    if (c >= '0' && c <= '9') {
      v = v * 10 + (c - '0');
    }
  }
  return (int32_t)v;
}

/* Print a decimal int literal slice with its full 64-bit value (the Li int
 * cells are 32-bit, so big literals are printed from the source span at
 * emission time to match the C++ i64 MIR dump fields). */
int32_t li_rt_mir_int64_out(const char* text, int32_t start, int32_t end) {
  if (text == NULL || start < 0 || end <= start) {
    fputc('0', stdout);
    return 0;
  }
  int64_t v = 0;
  int32_t i = start;
  if (text[i] == '-') {
    ++i;
  }
  for (; i < end; ++i) {
    const unsigned char c = (unsigned char)text[i];
    if (c >= '0' && c <= '9') {
      v = v * 10 + (c - '0');
    }
  }
  if (start < end && text[start] == '-') {
    printf("%lld", (long long)-v);
  } else {
    printf("%lld", (long long)v);
  }
  return 0;
}

int32_t li_rt_mir_esc(const char* text, int32_t start, int32_t end) {
  if (text == NULL || start < 0) {
    return 0;
  }
  if (end > start) {
    for (int32_t i = start; i < end; ++i) {
      const char c = text[i];
      switch (c) {
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        case ' ': fputs("\\x20", stdout); break;
        default: fputc(c, stdout); break;
      }
    }
  }
  return 0;
}

int32_t li_rt_mir_str(const char* s) {
  if (s != NULL) {
    fputs(s, stdout);
  }
  return 0;
}

int32_t li_rt_mir_label(int32_t code, int32_t v) {
  const char* p = NULL;
  switch (code) {
    case 0: p = "__t"; break;
    case 1: p = "else_"; break;
    case 2: p = "merge_"; break;
    case 3: p = "while_head_"; break;
    case 4: p = "while_exit_"; break;
    case 5: p = "for_head_"; break;
    case 6: p = "for_exit_"; break;
    default: break;
  }
  if (p != NULL) {
    fputs(p, stdout);
  }
  printf("%d", (int)v);
  return 0;
}

int32_t li_rt_mir_literal(int32_t idx) {
  const char* names[] = {
    "li_rt_sqrt",
    "li_rt_abs",
    "li_rt_clamp",
  };
  int32_t count = (int32_t)(sizeof(names) / sizeof(names[0]));
  if (idx >= 0 && idx < count) {
    fputs(names[idx], stdout);
  }
  return 0;
}

/* Object-field mangled-name registry (Layer 5 MIR parity): the C++ dump
 * prints object slots as `__li_o_<base>_<field>` directly in name cells, but
 * the self-hosted walker's name cells only carry two source positions. So the
 * walker registers the mangled name once (li_rt_mir_objname_add) and name
 * cells reference it by index (li_rt_mir_objname_out). */
#define LI_RT_OBJNAME_MAX 512
static const char* li_rt_objname_text[LI_RT_OBJNAME_MAX];
static int32_t li_rt_objname_bs[LI_RT_OBJNAME_MAX];
static int32_t li_rt_objname_be[LI_RT_OBJNAME_MAX];
static int32_t li_rt_objname_fs[LI_RT_OBJNAME_MAX];
static int32_t li_rt_objname_fe[LI_RT_OBJNAME_MAX];
static int32_t li_rt_objname_n = 0;

int32_t li_rt_mir_objname_add(const char* text, int32_t bs, int32_t be,
                              int32_t fs, int32_t fe) {
  for (int32_t i = 0; i < li_rt_objname_n; ++i) {
    if (li_rt_objname_text[i] == text &&
        li_rt_objname_bs[i] == bs && li_rt_objname_be[i] == be &&
        li_rt_objname_fs[i] == fs && li_rt_objname_fe[i] == fe) {
      return i;
    }
  }
  if (li_rt_objname_n >= LI_RT_OBJNAME_MAX) {
    return 0;
  }
  li_rt_objname_text[li_rt_objname_n] = text;
  li_rt_objname_bs[li_rt_objname_n] = bs;
  li_rt_objname_be[li_rt_objname_n] = be;
  li_rt_objname_fs[li_rt_objname_n] = fs;
  li_rt_objname_fe[li_rt_objname_n] = fe;
  const int32_t idx = li_rt_objname_n;
  li_rt_objname_n += 1;
  return idx;
}

int32_t li_rt_mir_objname_out(int32_t idx) {
  if (idx < 0 || idx >= li_rt_objname_n) {
    return 0;
  }
  /* Synthesized-name entries (be < 0) print verbatim; used for parallel-for
   * callee cells whose text is not a mangled object field. */
  if (li_rt_objname_be[idx] < 0) {
    fputs(li_rt_objname_text[idx], stdout);
    return 0;
  }
  fputs("__li_o_", stdout);
  li_rt_mir_esc(li_rt_objname_text[idx], li_rt_objname_bs[idx], li_rt_objname_be[idx]);
  fputs("_", stdout);
  li_rt_mir_esc(li_rt_objname_text[idx], li_rt_objname_fs[idx], li_rt_objname_fe[idx]);
  return 0;
}

/* Register a fully synthesized name (e.g. __li_par_main_0) for a callee cell. */
int32_t li_rt_mir_synth_name_add(const char* text) {
  return li_rt_mir_objname_add(text, 0, -1, 0, 0);
}

/* Create a nested objname: parent printed name + "_" + field text.
 * Returns a synthesized entry (be < 0) printed verbatim by objname_out. */
int32_t li_rt_mir_objname_add_nested(int32_t parent_idx, const char* text,
                                     int32_t fs, int32_t fe) {
  if (parent_idx < 0 || parent_idx >= li_rt_objname_n) {
    return li_rt_mir_objname_add(text, 0, -1, 0, 0);
  }
  /* Build: __li_o_<base>_<field>_<new_field> by printing parent + "_" + field */
  char buf[1024];
  int off = 0;
  /* Print parent as it would appear */
  if (li_rt_objname_be[parent_idx] < 0) {
    /* Synthesized parent: just copy its text */
    const char* pt = li_rt_objname_text[parent_idx];
    while (*pt && off < (int)sizeof(buf) - 256) buf[off++] = *pt++;
  } else {
    /* Normal parent: __li_o_<base>_<field> */
    const char* prefix = "__li_o_";
    for (const char* p = prefix; *p; p++) buf[off++] = *p;
    const char* src = li_rt_objname_text[parent_idx];
    for (int i = li_rt_objname_bs[parent_idx]; i < li_rt_objname_be[parent_idx]; i++) {
      buf[off++] = src[i];
    }
    buf[off++] = '_';
    for (int i = li_rt_objname_fs[parent_idx]; i < li_rt_objname_fe[parent_idx]; i++) {
      buf[off++] = src[i];
    }
  }
  buf[off++] = '_';
  /* Append new field text */
  for (int i = fs; i < fe && off < (int)sizeof(buf) - 1; i++) {
    buf[off++] = text[i];
  }
  buf[off] = '\0';
  return li_rt_mir_synth_name_add(strdup(buf));
}

void li_rt_mir_objname_clear(void) { li_rt_objname_n = 0; }

/* Register a compound prefix for objname recursive copy: builds
 * __li_o_<base>_<field> or __li_o___cr<N>_<field> as a synth entry.
 * src_is_cr=1 uses __li_o___cr<crid> prefix, otherwise __li_o_<base> prefix. */
int32_t li_rt_mir_objname_reg_prefix(const char* text, int32_t base_s, int32_t base_e,
                                     int32_t fs, int32_t fe,
                                     int32_t src_is_cr, int32_t crid,
                                     const char* fsrc) {
  char buf[1024];
  int off = 0;
  if (src_is_cr == 1) {
    const char* prefix = "__li_o___cr";
    for (const char* p = prefix; *p; p++) buf[off++] = *p;
    /* Append crid as decimal */
    char tmp[16]; int tn = 0;
    int v = crid;
    if (v == 0) { tmp[tn++] = '0'; } else {
      while (v > 0) { tmp[tn++] = '0' + v % 10; v /= 10; }
    }
    for (int i = tn - 1; i >= 0; i--) buf[off++] = tmp[i];
  } else if (src_is_cr == 3) {
    const char* prefix = "__li_o_wb";
    for (const char* p = prefix; *p; p++) buf[off++] = *p;
    char tmp[16]; int tn = 0;
    int v = crid;
    if (v == 0) { tmp[tn++] = '0'; } else {
      while (v > 0) { tmp[tn++] = '0' + v % 10; v /= 10; }
    }
    for (int i = tn - 1; i >= 0; i--) buf[off++] = tmp[i];
  } else {
    const char* prefix = "__li_o_";
    for (const char* p = prefix; *p; p++) buf[off++] = *p;
    for (int i = base_s; i < base_e; i++) buf[off++] = text[i];
  }
  buf[off++] = '_';
  const char* ftext = (fsrc != NULL) ? fsrc : text;
  for (int i = fs; i < fe; i++) buf[off++] = ftext[i];
  buf[off] = '\0';
  return li_rt_mir_synth_name_add(strdup(buf));
}

#define LI_RT_FIELDPATH_MAX 1024
static int32_t li_rt_fieldpath_parent[LI_RT_FIELDPATH_MAX];
static const char* li_rt_fieldpath_text[LI_RT_FIELDPATH_MAX];
static int32_t li_rt_fieldpath_s[LI_RT_FIELDPATH_MAX];
static int32_t li_rt_fieldpath_e[LI_RT_FIELDPATH_MAX];
static int32_t li_rt_fieldpath_n = 1;

int32_t li_rt_mir_fieldpath_add(int32_t parent, const char* text, int32_t s, int32_t e) {
  if (parent < 0 || parent >= li_rt_fieldpath_n || text == NULL || e <= s) return 0;
  if (li_rt_fieldpath_n >= LI_RT_FIELDPATH_MAX) return 0;
  const int32_t idx = li_rt_fieldpath_n++;
  li_rt_fieldpath_parent[idx] = parent;
  li_rt_fieldpath_text[idx] = text;
  li_rt_fieldpath_s[idx] = s;
  li_rt_fieldpath_e[idx] = e;
  return idx;
}

int32_t li_rt_mir_fieldpath_out(int32_t idx) {
  if (idx <= 0 || idx >= li_rt_fieldpath_n) return 0;
  int32_t chain[64];
  int32_t n = 0;
  int32_t cur = idx;
  while (cur > 0 && cur < li_rt_fieldpath_n && n < 64) {
    chain[n++] = cur;
    cur = li_rt_fieldpath_parent[cur];
  }
  for (int32_t i = n - 1; i >= 0; --i) {
    if (i != n - 1) fputc('_', stdout);
    li_rt_mir_esc(li_rt_fieldpath_text[chain[i]], li_rt_fieldpath_s[chain[i]],
                  li_rt_fieldpath_e[chain[i]]);
  }
  return 0;
}




/* Proc-name registry for cross-file call resolution.
 * Stores one NUL-terminated name per proc ID (max 128 procs, 63 chars each).
 * Used by the self-hosted MIR walker so that imported proc names survive
 * token-buffer re-lexing across source files. */
#define LI_RT_PNAME_MAX 512
static char li_rt_pname_buf[LI_RT_PNAME_MAX * 64];
static int32_t li_rt_pname_len[LI_RT_PNAME_MAX];
static int32_t li_rt_pname_ext[LI_RT_PNAME_MAX];  /* is_extern flag */
static int32_t li_rt_pname_count = 0;

int32_t li_rt_mir_pname_store(int32_t pid, const char* src, int32_t s, int32_t e) {
  if (pid < 0 || pid >= LI_RT_PNAME_MAX || src == NULL) return 0;
  int32_t len = e - s;
  if (len > 63) len = 63;
  if (len < 0) len = 0;
  char* dst = li_rt_pname_buf + pid * 64;
  for (int32_t i = 0; i < len; i++) dst[i] = src[s + i];
  dst[len] = '\0';
  li_rt_pname_len[pid] = len;
  if (pid >= li_rt_pname_count) li_rt_pname_count = pid + 1;
  return 0;
}

int32_t li_rt_mir_pname_eq(int32_t pid, const char* src, int32_t s, int32_t e) {
  if (pid < 0 || pid >= LI_RT_PNAME_MAX || src == NULL) return 0;
  int32_t len = e - s;
  if (len != li_rt_pname_len[pid]) return 0;
  const char* stored = li_rt_pname_buf + pid * 64;
  for (int32_t i = 0; i < len; i++) {
    if (stored[i] != src[s + i]) return 0;
  }
  return 1;
}

int32_t li_rt_mir_pname_set_extern(int32_t pid, int32_t is_extern) {
  if (pid < 0 || pid >= LI_RT_PNAME_MAX) return 0;
  li_rt_pname_ext[pid] = is_extern;
  return 0;
}

int32_t li_rt_mir_pname_is_extern(int32_t pid) {
  if (pid < 0 || pid >= LI_RT_PNAME_MAX) return 0;
  return li_rt_pname_ext[pid];
}

int32_t li_rt_mir_pname_print(int32_t pid) {
  if (pid < 0 || pid >= LI_RT_PNAME_MAX) return 0;
  fwrite(li_rt_pname_buf + pid * 64, 1, (size_t)li_rt_pname_len[pid], stdout);
  return 0;
}

/* Return-type name registry (parallel to pname): stores the resolved return
 * type name per proc so the call-site lowering can find the object layout of
 * a callee that returns an object (C++ callee_ret_obj path). The name is
 * copied at collect time, so it survives token-buffer re-lexing. */
static char li_rt_retname_buf[LI_RT_PNAME_MAX * 64];
static int32_t li_rt_retname_len[LI_RT_PNAME_MAX];

int32_t li_rt_mir_retname_store(int32_t pid, const char* src, int32_t s, int32_t e) {
  if (pid < 0 || pid >= LI_RT_PNAME_MAX || src == NULL) return 0;
  int32_t len = e - s;
  if (len > 63) len = 63;
  if (len < 0) len = 0;
  char* dst = li_rt_retname_buf + pid * 64;
  for (int32_t i = 0; i < len; i++) dst[i] = src[s + i];
  dst[len] = '\0';
  li_rt_retname_len[pid] = len;
  return 0;
}

int32_t li_rt_mir_retname_hash(int32_t pid) {
  if (pid < 0 || pid >= LI_RT_PNAME_MAX) return 0;
  const char* p = li_rt_retname_buf + pid * 64;
  int32_t len = li_rt_retname_len[pid];
  /* Must match the Li walker's mir_name_hash exactly: Li ints are 32-bit,
   * so `h * 31 + c` wraps at int32 before `h % 2147483647` (C-style
   * truncating mod). The itmp object registry stores these wrapped hashes,
   * so a mismatched algorithm makes mir_obj_find_rettype miss every type. */
  int32_t h = 0;
  for (int32_t i = 0; i < len; i++) {
    h = (int32_t)((int64_t)h * 31 + (int64_t)(unsigned char)p[i]);
    h = h % 2147483647;
  }
  return h;
}

int32_t li_rt_mir_retname_len(int32_t pid) {
  if (pid < 0 || pid >= LI_RT_PNAME_MAX) return 0;
  return li_rt_retname_len[pid];
}

/* Parallel-for synthesized function name: prints `__li_par_<proc>_<counter>`,
 * matching the C++ lower_to_mir ParallelFor naming scheme. */
int32_t li_rt_mir_par_name_print(int32_t pid, int32_t counter) {
  fputs("__li_par_", stdout);
  if (pid >= 0 && pid < LI_RT_PNAME_MAX) {
    fwrite(li_rt_pname_buf + pid * 64, 1, (size_t)li_rt_pname_len[pid], stdout);
  }
  printf("_%d", (int)counter);
  return 0;
}

static int32_t li_rt_par_counter = 0;

int32_t li_rt_mir_par_counter_next(void) {
  const int32_t v = li_rt_par_counter;
  li_rt_par_counter += 1;
  return v;
}

void li_rt_mir_par_counter_reset(void) { li_rt_par_counter = 0; }

/* Build `__li_par_<proc>_<counter>` from the pname registry and register it
 * as a synthesized callee-cell name; returns the cell index. */
int32_t li_rt_mir_par_callee_register(int32_t pid, int32_t counter) {
  char tmp[128];
  int32_t plen = 0;
  if (pid >= 0 && pid < LI_RT_PNAME_MAX) {
    plen = li_rt_pname_len[pid];
    if (plen > 48) plen = 48;
  }
  int n = snprintf(tmp, sizeof(tmp), "__li_par_%.*s_%d",
                   (int)plen,
                   (pid >= 0 && pid < LI_RT_PNAME_MAX) ? li_rt_pname_buf + pid * 64 : "",
                   (int)counter);
  if (n <= 0) return 0;
  return li_rt_mir_synth_name_add(strdup(tmp));
}

/* MIR hold buffers: the C++ lowerer pushes each `parallel for` synthesized
 * function into module.functions while lowering its enclosing procedure, so
 * the dump shows __li_par_* functions BEFORE the enclosing FN. The streaming
 * self-hosted walker emits text in walk order, so it captures both streams in
 * memory buffers (0 = enclosing proc text, 1 = par fn text) and replays par
 * first. Only procs that actually contain a parallel-for pay for buffering. */
static FILE* li_rt_hold_real = NULL;
static FILE* li_rt_hold_fp[2] = {NULL, NULL};
static char* li_rt_hold_buf[2] = {NULL, NULL};
static size_t li_rt_hold_len[2] = {0, 0};

int32_t li_rt_mir_hold_open(void) {
  if (li_rt_hold_real == NULL) {
    fflush(stdout);
    li_rt_hold_real = stdout;
    li_rt_hold_fp[0] = open_memstream(&li_rt_hold_buf[0], &li_rt_hold_len[0]);
    li_rt_hold_fp[1] = open_memstream(&li_rt_hold_buf[1], &li_rt_hold_len[1]);
    stdout = li_rt_hold_fp[0];
  }
  return 0;
}

int32_t li_rt_mir_hold_swap(int32_t which) {
  if (li_rt_hold_real == NULL) return 1;
  fflush(stdout);
  stdout = (which == 1) ? li_rt_hold_fp[1] : li_rt_hold_fp[0];
  return 0;
}

int32_t li_rt_mir_hold_flush_close(void) {
  if (li_rt_hold_real == NULL) return 0;
  fflush(stdout);
  FILE* real = li_rt_hold_real;
  fclose(li_rt_hold_fp[0]);
  fclose(li_rt_hold_fp[1]);
  /* Par functions replay before the enclosing procedure's text. */
  if (li_rt_hold_buf[1] != NULL) {
    fwrite(li_rt_hold_buf[1], 1, li_rt_hold_len[1], real);
  }
  if (li_rt_hold_buf[0] != NULL) {
    fwrite(li_rt_hold_buf[0], 1, li_rt_hold_len[0], real);
  }
  fflush(real);
  free(li_rt_hold_buf[0]);
  free(li_rt_hold_buf[1]);
  li_rt_hold_buf[0] = NULL;
  li_rt_hold_buf[1] = NULL;
  li_rt_hold_len[0] = 0;
  li_rt_hold_len[1] = 0;
  li_rt_hold_fp[0] = NULL;
  li_rt_hold_fp[1] = NULL;
  li_rt_hold_real = NULL;
  stdout = real;
  return 0;
}

static int li_argc = 0;
static char** li_argv = NULL;

void li_rt_set_args(int argc, char** argv) {
  li_argc = argc;
  li_argv = argv;
}

int li_rt_argc(void) { return li_argc; }

const char* li_rt_argv(int index) {
  if (index < 0 || index >= li_argc || li_argv == NULL) {
    return "";
  }
  return li_argv[index];
}

typedef struct {
  long long begin;
  long long end;
} LiParChunk;

typedef struct {
  void (*body)(long long);
  LiParChunk chunk;
} LiParWorkerArg;

#if !defined(_WIN32)
static void* li_par_worker(void* raw) {
  LiParWorkerArg* arg = (LiParWorkerArg*)raw;
  for (long long i = arg->chunk.begin; i < arg->chunk.end; ++i) {
    arg->body(i);
  }
  return NULL;
}
#endif

static int li_warn_omp_alias_once(void) {
  static int warned = 0;
  if (!warned) {
    fprintf(stderr,
            "lic: warning: li_omp_parallel_for_i64 is deprecated; use li_parallel_for_i64 "
            "(native pthread pool)\n");
    warned = 1;
  }
  return 0;
}

static int li_resolve_team_size(int team_size) {
  if (team_size > 0) {
    return team_size;
  }
  const char* nt = getenv("LI_OMP_THREADS");
  if (nt && *nt) {
    int threads = atoi(nt);
    if (threads > 0) {
      return threads;
    }
  }
#if defined(_WIN32)
  return 1;
#else
  long cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (cores < 1) {
    return 1;
  }
  if (cores > LI_MAX_THREADS) {
    return LI_MAX_THREADS;
  }
  return (int)cores;
#endif
}

static int li_clamp_team(int team_size, long long trip_count) {
  team_size = li_resolve_team_size(team_size);
  if (team_size > LI_MAX_THREADS) {
    team_size = LI_MAX_THREADS;
  }
  if (trip_count < (long long)team_size) {
    team_size = (int)trip_count;
  }
  if (team_size < 1) {
    team_size = 1;
  }
  return team_size;
}

void li_parallel_for_i64(long long start, long long end, void (*body)(long long),
                         int team_size) {
  const long long trip = end - start;
  if (trip <= 0 || body == NULL) {
    return;
  }
  if (trip == 1) {
    body(start);
    return;
  }

  team_size = li_clamp_team(team_size, trip);
  if (team_size <= 1) {
    for (long long i = start; i < end; ++i) {
      body(i);
    }
    return;
  }

#if defined(_WIN32)
  (void)team_size;
  for (long long i = start; i < end; ++i) {
    body(i);
  }
#else
  pthread_t threads[LI_MAX_THREADS];
  LiParWorkerArg args[LI_MAX_THREADS];
  const long long base = trip / team_size;
  const long long rem = trip % team_size;
  int launched = 0;
  long long cur = start;
  for (int w = 0; w < team_size; ++w) {
    const long long len = base + (w < (int)rem ? 1 : 0);
    if (len <= 0) {
      continue;
    }
    args[launched].body = body;
    args[launched].chunk.begin = cur;
    args[launched].chunk.end = cur + len;
    cur += len;
    if (pthread_create(&threads[launched], NULL, li_par_worker, &args[launched]) != 0) {
      for (long long i = args[launched].chunk.begin; i < args[launched].chunk.end; ++i) {
        body(i);
      }
      continue;
    }
    ++launched;
  }
  for (int w = 0; w < launched; ++w) {
    pthread_join(threads[w], NULL);
  }
#endif
}

void li_omp_parallel_for_i64(long long start, long long end, void (*body)(long long)) {
  (void)li_warn_omp_alias_once();
  li_parallel_for_i64(start, end, body, 0);
}

int32_t li_rt_floor_div_i32(int32_t a, int32_t b) {
  if (b == 0) {
    li_panic("division by zero");
  }
  int32_t q = a / b;
  int32_t r = a % b;
  if (r != 0 && ((r < 0) != (b < 0))) {
    --q;
  }
  return q;
}

int32_t li_rt_pow_i32(int32_t base, int32_t exp) {
  if (exp < 0) {
    return 0;
  }
  int32_t out = 1;
  for (int32_t i = 0; i < exp; ++i) {
    out *= base;
  }
  return out;
}

double li_rt_sqrt(double x) {
#if defined(_WIN32)
  return sqrt(x);
#else
  return __builtin_sqrt(x);
#endif
}

double li_rt_sin(double x) { return sin(x); }

double li_rt_cos(double x) { return cos(x); }

double li_rt_atan2(double y, double x) { return atan2(y, x); }

double li_rt_exp(double x) { return exp(x); }

double li_rt_log(double x) { return log(x); }

double li_rt_hypot(double x, double y) {
#if defined(_WIN32)
  return hypot(x, y);
#else
  return __builtin_hypot(x, y);
#endif
}

double li_rt_expm1(double x) {
#if defined(_WIN32)
  return expm1(x);
#else
  return __builtin_expm1(x);
#endif
}

double li_rt_log1p(double x) { return log1p(x); }

void li_rt_print_f64(double v) { printf("%.17g\n", v); }

void li_rt_volatile_sink_f64(double v) {
  const char* emit = getenv("LI_PRINT_SINK_F64");
  if (emit != NULL && emit[0] == '1' && emit[1] == '\0') {
    printf("%.17g\n", v);
  }
  volatile double sink = v;
  (void)sink;
}

int32_t li_rt_str_byte_at(const char* s, int32_t i) {
  if (s == NULL || i < 0) {
    li_panic("li_rt_str_byte_at: bad args");
  }
  const size_t len = strlen(s);
  if ((size_t)i >= len) {
    li_panic("li_rt_str_byte_at: out of bounds");
  }
  return (int32_t)(unsigned char)s[i];
}

int32_t li_rt_str_prefix_is_get(const char* s) {
  if (s == NULL) {
    return 0;
  }
  const unsigned char* p = (const unsigned char*)s;
  return (p[0] == (unsigned char)'G' && p[1] == (unsigned char)'E' && p[2] == (unsigned char)'T') ? 1 : 0;
}

int32_t li_rt_http_parse_request_len_tag(const char* s, int32_t max_header_block, int32_t max_body) {
  (void)max_body;
  if (s == NULL || max_header_block <= 0) {
    return 0;
  }
  const size_t len = strlen(s);
  if (len > (size_t)max_header_block) {
    return 0;
  }
  const int32_t n = (int32_t)len;
  if (n < 3) {
    return n;
  }
  return n + li_rt_str_prefix_is_get(s);
}

int32_t li_rt_str_len(const char* s) {
  if (s == NULL) {
    return 0;
  }
  return (int32_t)strlen(s);
}

int32_t li_rt_str_char_at(const char* s, int32_t i) {
  if (s == NULL || i < 0) {
    return -1;
  }
  const size_t len = strlen(s);
  if ((size_t)i >= len) {
    return -1;
  }
  return (int32_t)(unsigned char)s[i];
}

int32_t li_rt_str_eq(const char* a, const char* b) {
  if (a == NULL || b == NULL) {
    return 0;
  }
  return strcmp(a, b) == 0 ? 1 : 0;
}

int32_t li_rt_str_prefix(const char* s, const char* prefix) {
  if (s == NULL || prefix == NULL) {
    return 0;
  }
  const size_t plen = strlen(prefix);
  return strncmp(s, prefix, plen) == 0 ? 1 : 0;
}

int32_t li_rt_str_contains(const char* s, const char* needle) {
  if (s == NULL || needle == NULL) {
    return 0;
  }
  return strstr(s, needle) != NULL ? 1 : 0;
}

const char* li_rt_read_file(const char* path) {
  if (path == NULL) {
    return NULL;
  }
  FILE* f = fopen(path, "rb");
  if (f == NULL) {
    return NULL;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  const long sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    return NULL;
  }
  rewind(f);
  char* buf = (char*)malloc((size_t)sz + 1);
  if (buf == NULL) {
    fclose(f);
    li_panic("li_rt_read_file: alloc failed");
  }
  const size_t got = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[got] = '\0';
  return buf;
}


static int32_t li_rt_studio_profile_match_name(const char* name) {
  if (name == NULL) {
    return 0;
  }
  if (li_rt_str_eq(name, "game")) {
    return 1;
  }
  if (li_rt_str_eq(name, "sim_rl")) {
    return 2;
  }
  if (li_rt_str_eq(name, "sim_automotive")) {
    return 3;
  }
  if (li_rt_str_eq(name, "sim_robotics")) {
    return 4;
  }
  if (li_rt_str_eq(name, "sim_additive")) {
    return 5;
  }
  if (li_rt_str_eq(name, "sim_scientific")) {
    return 6;
  }
  if (li_rt_str_eq(name, "sim_drug_design")) {
    return 7;
  }
  return 0;
}

int32_t li_rt_studio_profile_from_name(const char* name) {
  return li_rt_studio_profile_match_name(name);
}

static int32_t li_rt_studio_mcp_tool_match_name(const char* name) {
  if (name == NULL) {
    return 0;
  }
  if (li_rt_str_eq(name, "world_scaffold")) {
    return 1;
  }
  if (li_rt_str_eq(name, "sim_set_profile")) {
    return 2;
  }
  if (li_rt_str_eq(name, "lic_check")) {
    return 3;
  }
  if (li_rt_str_eq(name, "lic_build")) {
    return 4;
  }
  if (li_rt_str_eq(name, "publish_bundle")) {
    return 5;
  }
  if (li_rt_str_eq(name, "am_export_print")) {
    return 6;
  }
  if (li_rt_str_eq(name, "chem_dft_run")) {
    return 7;
  }
  if (li_rt_str_eq(name, "studio_adaptive_layout")) {
    return 8;
  }
  return 0;
}

int32_t li_rt_studio_mcp_tool_from_name(const char* name) {
  return li_rt_studio_mcp_tool_match_name(name);
}

const char* li_rt_studio_mcp_tool_name(int32_t tool_id) {
  switch (tool_id) {
    case 1:
      return "world_scaffold";
    case 2:
      return "sim_set_profile";
    case 3:
      return "lic_check";
    case 4:
      return "lic_build";
    case 5:
      return "publish_bundle";
    case 6:
      return "am_export_print";
    case 7:
      return "chem_dft_run";
    case 8:
      return "studio_adaptive_layout";
    default:
      return "";
  }
}

static int32_t g_studio_timeline_playing = 0;
static float g_studio_timeline_playhead_pct = 0.35f;

int32_t li_rt_studio_timeline_playing(void) { return g_studio_timeline_playing; }

int32_t li_rt_studio_timeline_toggle_play(void) {
  g_studio_timeline_playing = g_studio_timeline_playing ? 0 : 1;
  return g_studio_timeline_playing;
}

int32_t li_rt_studio_timeline_tick_frame(void) {
  if (!g_studio_timeline_playing) {
    return 0;
  }
  g_studio_timeline_playhead_pct += 0.01f;
  if (g_studio_timeline_playhead_pct > 1.0f) {
    g_studio_timeline_playhead_pct = 1.0f;
  }
  return 1;
}

float li_rt_studio_timeline_playhead_pct(void) { return g_studio_timeline_playhead_pct; }

int32_t li_rt_studio_timeline_reset_mock(void) {
  g_studio_timeline_playing = 0;
  g_studio_timeline_playhead_pct = 0.35f;
  return 0;
}

static int32_t g_studio_viewport_error_kind = 0;

int32_t li_rt_studio_viewport_error_kind(void) { return g_studio_viewport_error_kind; }

int32_t li_rt_studio_viewport_error_set_mock(int32_t kind) {
  if (kind < 0 || kind > 2) {
    return g_studio_viewport_error_kind;
  }
  g_studio_viewport_error_kind = kind;
  return kind;
}

int32_t li_rt_studio_viewport_error_retry(void) {
  g_studio_viewport_error_kind = 0;
  return 0;
}

int32_t li_rt_studio_parse_toml_profile_line(const char* line) {
  if (line == NULL) {
    return 0;
  }
  const char* key = "profile";
  const char* p = strstr(line, key);
  if (p == NULL) {
    return 0;
  }
  p += strlen(key);
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  if (*p != '=') {
    return 0;
  }
  p++;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  if (*p != '"') {
    return 0;
  }
  p++;
  const char* start = p;
  while (*p != '\0' && *p != '"') {
    p++;
  }
  if (*p != '"') {
    return 0;
  }
  char buf[64];
  const size_t n = (size_t)(p - start);
  if (n == 0 || n >= sizeof(buf)) {
    return 0;
  }
  memcpy(buf, start, n);
  buf[n] = '\0';
  return li_rt_studio_profile_match_name(buf);
}


int32_t li_rt_studio_demo_profile_from_env(void) {
  const char* v = getenv("STUDIO_DEMO_PROFILE");
  if (v == NULL || v[0] == '\0') {
    return 1;
  }
  const int32_t id = li_rt_studio_profile_match_name(v);
  if (id == 0) {
    return 1;
  }
  return id;
}

/* PH-HW HW-1 — lig.present trusted edge (SDL host; wgpu-rs readback not in-tree). */
static int32_t g_lig_host_present_active = 0;
static int32_t g_lig_native_pixels = 0;
static float g_lig_present_dt_ms = 16.667f;
static int32_t g_lig_surface_ok = 0;

static int32_t li_rt_lig_env_host_present(void) {
  const char* v = getenv("LIG_HOST_PRESENT");
  return (v != NULL && v[0] == '1' && v[1] == '\0') ? 1 : 0;
}

static void li_rt_lig_refresh_host_active(void) {
  g_lig_host_present_active = li_rt_lig_env_host_present();
}

static int32_t g_studio_shell_pointer_down = 0;
static float g_studio_shell_pointer_x = 0.0f;
static float g_studio_shell_pointer_y = 0.0f;
static int32_t g_studio_shell_key_escape = 0;
static int32_t g_studio_shell_key_cmd_k = 0;
static int32_t g_studio_shell_key_digit = 0;

static int32_t li_rt_lig_try_sdl_present_host(int32_t viewport_w, int32_t viewport_h) {
  const char* bin = getenv("STUDIO_SHELL_PRESENT_HOST_BIN");
  if (bin == NULL || bin[0] == '\0') {
    return 0;
  }
  if (viewport_w <= 0 || viewport_h <= 0) {
    return 0;
  }
  char cmd[640];
  snprintf(cmd, sizeof(cmd), "%s --width %d --height %d", bin, (int)viewport_w, (int)viewport_h);
  if (system(cmd) != 0) {
    return 0;
  }
  g_lig_native_pixels = 1;
  g_lig_surface_ok = 1;
  g_lig_present_dt_ms = 16.667f;
  return 1;
}

int32_t li_rt_lig_host_present_active(void) {
  li_rt_lig_refresh_host_active();
  return g_lig_host_present_active;
}

float li_rt_lig_host_present_dt_ms(void) {
  li_rt_lig_refresh_host_active();
  if (g_lig_host_present_active) {
    return g_lig_present_dt_ms;
  }
  return 16.667f;
}

int32_t li_rt_lig_host_native_pixels(void) {
  li_rt_lig_refresh_host_active();
  if (!g_lig_host_present_active) {
    return 0;
  }
  return g_lig_native_pixels;
}

int32_t li_rt_lig_wgpu_swapchain_create(int32_t viewport_w, int32_t viewport_h) {
  li_rt_lig_refresh_host_active();
  if (viewport_w <= 0 || viewport_h <= 0) {
    g_lig_surface_ok = 0;
    return 0;
  }
  if (g_lig_host_present_active) {
    if (li_rt_lig_try_sdl_present_host(viewport_w, viewport_h)) {
      return 1;
    }
    g_lig_surface_ok = 1;
    return 1;
  }
  g_lig_surface_ok = 0;
  return 1;
}

int32_t li_rt_lig_wgpu_present_frame(int32_t swapchain_ok) {
  li_rt_lig_refresh_host_active();
  if (!swapchain_ok) {
    return 0;
  }
  if (g_lig_host_present_active && g_lig_surface_ok) {
    g_lig_native_pixels = 1;
    g_lig_present_dt_ms = 16.667f;
    return 1;
  }
  g_lig_native_pixels = 0;
  return 1;
}

int32_t li_rt_studio_shell_input_pointer_down(void) {
  return g_studio_shell_pointer_down;
}
float li_rt_studio_shell_input_pointer_x(void) {
  return g_studio_shell_pointer_x;
}
float li_rt_studio_shell_input_pointer_y(void) {
  return g_studio_shell_pointer_y;
}
int32_t li_rt_studio_shell_input_key_escape(void) {
  return g_studio_shell_key_escape;
}
int32_t li_rt_studio_shell_input_key_cmd_k(void) {
  return g_studio_shell_key_cmd_k;
}
int32_t li_rt_studio_shell_input_key_digit(void) {
  return g_studio_shell_key_digit;
}

int32_t li_rt_studio_host_present_tick(int32_t viewport_w, int32_t viewport_h) {
  const char* mock = getenv("STUDIO_SHELL_INPUT_MOCK");
  if (mock != NULL && mock[0] != '\0') {
    g_studio_shell_key_cmd_k = (strstr(mock, "cmd_k") != NULL) ? 1 : 0;
    g_studio_shell_key_escape = (strstr(mock, "escape") != NULL) ? 1 : 0;
    const char* digit = strstr(mock, "digit=");
    if (digit != NULL) {
      int d = atoi(digit + 6);
      if (d >= 1 && d <= 5) {
        g_studio_shell_key_digit = d;
      }
    }
  }
  (void)li_rt_lig_wgpu_swapchain_create(viewport_w, viewport_h);
  return li_rt_lig_wgpu_present_frame(viewport_w > 0 && viewport_h > 0 ? 1 : 0);
}


/* PH-HW HW-0: lig device layer (was li-gpu stub). */
#define LI_RT_LIG_BACKEND_CUDA 1
#define LI_RT_LIG_BACKEND_ROCM 2
#define LI_RT_LIG_BACKEND_METAL 3
#define LI_RT_LIG_BACKEND_WEBGPU 4

static int32_t g_lig_selected_backend = LI_RT_LIG_BACKEND_WEBGPU;

static int32_t li_rt_lig_backend_probe_available(int32_t backend_id) {
  switch (backend_id) {
    case LI_RT_LIG_BACKEND_CUDA:
      return (getenv("CUDA_HOME") != NULL || getenv("CUDA_PATH") != NULL) ? 1 : 0;
    case LI_RT_LIG_BACKEND_ROCM:
      return (getenv("ROCM_PATH") != NULL || getenv("HIP_PATH") != NULL) ? 1 : 0;
    case LI_RT_LIG_BACKEND_METAL:
#if defined(__APPLE__)
      return 1;
#else
      return 0;
#endif
    case LI_RT_LIG_BACKEND_WEBGPU:
      return 1;
    default:
      return 0;
  }
}

static int32_t li_rt_lig_backend_match_name(const char* name) {
  if (name == NULL) {
    return 0;
  }
  if (strcmp(name, "cuda") == 0) {
    return LI_RT_LIG_BACKEND_CUDA;
  }
  if (strcmp(name, "rocm") == 0 || strcmp(name, "hip") == 0) {
    return LI_RT_LIG_BACKEND_ROCM;
  }
  if (strcmp(name, "metal") == 0) {
    return LI_RT_LIG_BACKEND_METAL;
  }
  if (strcmp(name, "webgpu") == 0 || strcmp(name, "wgpu") == 0) {
    return LI_RT_LIG_BACKEND_WEBGPU;
  }
  return 0;
}

int32_t li_rt_lig_device_kind(void) { return g_lig_selected_backend; }

int32_t li_rt_lig_backend_available(int32_t backend_id) {
  return li_rt_lig_backend_probe_available(backend_id);
}

int32_t li_rt_lig_backend_select_auto(void) {
#if defined(__APPLE__)
  if (li_rt_lig_backend_probe_available(LI_RT_LIG_BACKEND_METAL)) {
    g_lig_selected_backend = LI_RT_LIG_BACKEND_METAL;
    return LI_RT_LIG_BACKEND_METAL;
  }
#endif
  if (li_rt_lig_backend_probe_available(LI_RT_LIG_BACKEND_ROCM)) {
    g_lig_selected_backend = LI_RT_LIG_BACKEND_ROCM;
    return LI_RT_LIG_BACKEND_ROCM;
  }
  if (li_rt_lig_backend_probe_available(LI_RT_LIG_BACKEND_CUDA)) {
    g_lig_selected_backend = LI_RT_LIG_BACKEND_CUDA;
    return LI_RT_LIG_BACKEND_CUDA;
  }
  g_lig_selected_backend = LI_RT_LIG_BACKEND_WEBGPU;
  return LI_RT_LIG_BACKEND_WEBGPU;
}

int32_t li_rt_lig_present_surface_ok(void) {
  li_rt_lig_refresh_host_active();
  if (g_lig_host_present_active && g_lig_surface_ok) {
    return 1;
  }
  return 0;
}

static char li_rt_lig_cap_json_buf[192];

const char* li_rt_lig_capability_json(void) {
  snprintf(li_rt_lig_cap_json_buf, sizeof(li_rt_lig_cap_json_buf),
           "{\"lig_version\":2,\"device_kind\":%d,\"surface_ok\":%d}",
           (int)g_lig_selected_backend, (int)li_rt_lig_present_surface_ok());
  return li_rt_lig_cap_json_buf;
}

int32_t li_rt_lig_parse_toml_backend_line(const char* line) {
  if (line == NULL) {
    return 0;
  }
  const char* key = "backend";
  const char* p = strstr(line, key);
  if (p == NULL) {
    return 0;
  }
  p += strlen(key);
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  if (*p != '=') {
    return 0;
  }
  p++;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  char buf[32];
  const char* start = NULL;
  size_t n = 0;
  if (*p == '"') {
    p++;
    start = p;
    while (*p != '\0' && *p != '"') {
      p++;
    }
    if (*p != '"') {
      return 0;
    }
    n = (size_t)(p - start);
  } else {
    start = p;
    while ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_') {
      p++;
    }
    n = (size_t)(p - start);
  }
  if (n == 0 || n >= sizeof(buf)) {
    return 0;
  }
  memcpy(buf, start, n);
  buf[n] = '\0';
  const int32_t id = li_rt_lig_backend_match_name(buf);
  if (id != 0) {
    g_lig_selected_backend = id;
  }
  return id;
}

/* PH-GD-2 li-world: world_v1 name=... tick=N entity_count=M (text line, no binary). */
#define LI_RT_WORLD_NAME_MAX 64
#define LI_RT_WORLD_LINE_MAX 256

static char li_rt_world_line_buf[LI_RT_WORLD_LINE_MAX];
static struct {
  int32_t name_slot;
  int32_t tick;
  int32_t entity_count;
  int32_t valid;
} li_rt_world_parsed;

static const char* li_rt_world_token_for_slot(int32_t slot) {
  if (slot == 1) {
    return "arena";
  }
  return "default";
}

static int32_t li_rt_world_slot_for_token(const char* name) {
  if (name != NULL && strcmp(name, "arena") == 0) {
    return 1;
  }
  return 0;
}

int32_t li_rt_world_format_version(void) { return 1; }

const char* li_rt_world_serialize_slot(int32_t name_slot, int32_t tick, int32_t entity_count) {
  if (name_slot < 0 || name_slot > 1) {
    name_slot = 0;
  }
  if (tick < 0) {
    tick = 0;
  }
  if (entity_count < 0) {
    entity_count = 0;
  }
  snprintf(li_rt_world_line_buf, sizeof(li_rt_world_line_buf), "world_v1 name=%s tick=%d entity_count=%d",
           li_rt_world_token_for_slot(name_slot), (int)tick, (int)entity_count);
  return li_rt_world_line_buf;
}

int32_t li_rt_world_parse_line(const char* line) {
  li_rt_world_parsed.valid = 0;
  li_rt_world_parsed.name_slot = 0;
  li_rt_world_parsed.tick = 0;
  li_rt_world_parsed.entity_count = 0;
  if (line == NULL) {
    return 0;
  }
  char name[LI_RT_WORLD_NAME_MAX];
  int tick = 0;
  int entity_count = 0;
  if (sscanf(line, "world_v1 name=%63s tick=%d entity_count=%d", name, &tick, &entity_count) != 3) {
    return 0;
  }
  if (tick < 0 || entity_count < 0) {
    return 0;
  }
  li_rt_world_parsed.name_slot = li_rt_world_slot_for_token(name);
  li_rt_world_parsed.tick = (int32_t)tick;
  li_rt_world_parsed.entity_count = (int32_t)entity_count;
  li_rt_world_parsed.valid = 1;
  return 1;
}

int32_t li_rt_world_parsed_name_slot(void) {
  if (li_rt_world_parsed.valid == 0) {
    return 0;
  }
  return li_rt_world_parsed.name_slot;
}

int32_t li_rt_world_parsed_tick(void) {
  if (li_rt_world_parsed.valid == 0) {
    return 0;
  }
  return li_rt_world_parsed.tick;
}

int32_t li_rt_world_parsed_entity_count(void) {
  if (li_rt_world_parsed.valid == 0) {
    return 0;
  }
  return li_rt_world_parsed.entity_count;
}

int32_t li_rt_world_snapshot_eq_fields(int32_t an, int32_t at, int32_t ae, int32_t bn, int32_t bt,
                                     int32_t be) {
  return (an == bn && at == bt && ae == be) ? 1 : 0;
}

int32_t li_rt_world_roundtrip_fields(int32_t name_slot, int32_t tick, int32_t entity_count) {
  const char* line = li_rt_world_serialize_slot(name_slot, tick, entity_count);
  if (li_rt_world_parse_line(line) != 1) {
    return 0;
  }
  if (li_rt_world_parsed.name_slot != name_slot) {
    return 0;
  }
  if (li_rt_world_parsed.tick != tick) {
    return 0;
  }
  if (li_rt_world_parsed.entity_count != entity_count) {
    return 0;
  }
  return 1;
}

int32_t li_rt_path_exact(const char* path, const char* want) {
  return li_rt_str_eq(path, want);
}

int32_t li_rt_path_prefix(const char* path, const char* prefix) {
  if (path == NULL || prefix == NULL || prefix[0] == '\0') {
    return 0;
  }
  const size_t plen = strlen(prefix);
  const size_t pathlen = strlen(path);
  if (pathlen < plen) {
    return 0;
  }
  if (strncmp(path, prefix, plen) != 0) {
    return 0;
  }
  if (pathlen == plen) {
    return 1;
  }
  if (path[plen] == '/') {
    return 1;
  }
  return 0;
}

int32_t li_rt_match_route_fixture(const char* method, const char* path) {
  if (method == NULL || path == NULL) {
    return 0;
  }
  if (strcmp(method, "GET") == 0) {
    if (li_rt_path_exact(path, "/health")) {
      return 1;
    }
    if (li_rt_path_prefix(path, "/v1")) {
      return 3;
    }
    return 0;
  }
  if (strcmp(method, "POST") == 0) {
    if (li_rt_path_prefix(path, "/v1")) {
      return 2;
    }
    return 0;
  }
  return 0;
}

void li_async_frame_enter(void) {}

void li_async_frame_leave(void) {}
