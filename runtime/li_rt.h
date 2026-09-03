#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void li_panic(const char* msg);
void li_bounds_fail(void);
void li_rt_print_int(int32_t value);
void li_rt_print_str(const char* s);
/* Emit one lexer token as `kind<TAB>text[start,end)<NL>` (self-hosted lexer). */
int32_t li_rt_emit_token(int32_t kind, const char* text, int32_t start, int32_t end);
/* Self-hosted AST dump primitives: stream one node line as `code field...`.
 * Used by bootstrap/lic/main.li `ast` to match the C++ `lic ast` dump. */
int32_t li_rt_ast_int(int32_t v);
int32_t li_rt_ast_text(const char* text, int32_t start, int32_t end);
int32_t li_rt_ast_space(void);
int32_t li_rt_ast_nl(void);
/* Self-hosted typechecker diagnostic seam: emit one `error [CODE]` line. */
int32_t li_rt_emit_err(int32_t code);
/* Self-hosted MIR dump primitives (Layer 5: `mir <file>` parity with the
 * C++ `lic mir` dump of lower_to_mir()). li_rt_mir_f64 parses a float
 * literal slice into a static table and returns its id; li_rt_mir_f64_fmt
 * prints that value with %.17g (the dump's canonical float format). */
int32_t li_rt_mir_f64(const char* text, int32_t start, int32_t end);
int32_t li_rt_mir_f64_fmt(int32_t id);
int32_t li_rt_mir_f64_of_int(int32_t v);
int32_t li_rt_mir_f64_neg(int32_t id);
/* Parse a decimal int literal slice (C++ lexer int_value semantics). */
int32_t li_rt_mir_int(const char* text, int32_t start, int32_t end);
/* Print a decimal int literal slice at its full 64-bit value (Li int cells
 * are 32-bit, so the MIR walker prints big literals from the source span).
 * `neg` is the walker's literal negated-marker (1 for `-N` folded into the
 * literal node; the span covers only the digits). */
int32_t li_rt_mir_int64_out(const char* text, int32_t start, int32_t end, int32_t neg);
/* Print an escaped string slice (mir_dump.cpp esc(): \\ \n \r \t space->\x20). */
int32_t li_rt_mir_esc(const char* text, int32_t start, int32_t end);
/* Print a fixed tag string (INS/FN/PARAM/ARG/MIR line headers). */
int32_t li_rt_mir_str(const char* s);
/* Print a counter name with a fixed prefix code: 0=__t 1=else_ 2=merge_
 * 3=while_head_ 4=while_exit_ 5=for_head_ 6=for_exit_. */
int32_t li_rt_mir_label(int32_t code, int32_t v);
/* Print a hardcoded callee name by index: 0=li_rt_sqrt, etc.
 * Used by the self-hosted MIR walker for extern call names. */
int32_t li_rt_mir_literal(int32_t idx);
/* Object-field mangled-name registry: register `__li_o_<base>_<field>` once
 * and reference it by index from name cells (mir_dump.cpp parity). */
int32_t li_rt_mir_objname_add(const char* text, int32_t bs, int32_t be,
                              int32_t fs, int32_t fe);
int32_t li_rt_mir_objname_out(int32_t idx);
void li_rt_mir_objname_clear(void);
/* Synthesized literal name registry (parallel-for callee cells). */
int32_t li_rt_mir_synth_name_add(const char* text);
/* Build and register `__li_par_<proc>_<counter>`; returns the cell index. */
int32_t li_rt_mir_par_callee_register(int32_t pid, int32_t counter);
/* Proc-name registry for cross-file call resolution in the self-hosted MIR walker.
 * Each collected proc's name is stored by pid; mir_proc_find can look up
 * whether a call-site name matches any collected proc without needing the
 * original source buffer.  (avoids threading pname arrays through lowering). */
int32_t li_rt_mir_pname_store(int32_t pid, const char* src, int32_t s, int32_t e);
int32_t li_rt_mir_pname_eq(int32_t pid, const char* src, int32_t s, int32_t e);
int32_t li_rt_mir_pname_set_extern(int32_t pid, int32_t is_extern);
int32_t li_rt_mir_pname_is_extern(int32_t pid);
/* Return-type name registry: stores the resolved return type name per proc
 * so object-returning callees resolve their object layout at call sites. */
int32_t li_rt_mir_retname_store(int32_t pid, const char* src, int32_t s, int32_t e);
int32_t li_rt_mir_retname_hash(int32_t pid);
int32_t li_rt_mir_retname_len(int32_t pid);
/* Parallel-for synthesized function name: `__li_par_<proc>_<counter>`. */
int32_t li_rt_mir_par_name_print(int32_t pid, int32_t counter);
int32_t li_rt_mir_par_counter_next(void);
void li_rt_mir_par_counter_reset(void);
/* MIR output hold buffers for parallel-for reordering (par fns replay first). */
int32_t li_rt_mir_hold_open(void);
int32_t li_rt_mir_hold_swap(int32_t which);
int32_t li_rt_mir_hold_flush_close(void);
const char* li_rt_resolve_import(const char* file_path, const char* module);
/* Return the path that was last successfully resolved by li_rt_resolve_import. */
const char* li_rt_resolve_import_last_path(void);
/* Indexed import path store for transitive import resolution. */
void li_rt_import_paths_clear(void);
void li_rt_import_path_store(const char* path);
const char* li_rt_import_path_get(int idx);
/* Per-type source pointer store: the source where each registered Object type
 * was defined, so field name positions can be read from the correct buffer. */
void li_rt_type_src_store(int idx, const char* src);
/* Global proc-count accumulator: works around a Li compiler bug where
 * var array parameters are passed by value in some code paths, causing
 * the proc-count pn[0] to not propagate across nested mir_walk calls. */
int32_t li_rt_pn_get(void);
void li_rt_pn_set(int32_t v);
int32_t li_rt_pidx_get(void);
void li_rt_pidx_set(int32_t v);
/* Global param registry get/set (workaround var-array pass-by-value bug).
 * table: 0=ptok 1=pex 2=pp0 3=ppn 4=pret 5=ptok2 6=pty 7=pelems
 *         8=pef 9=pei 10=pvar 11=pmx 12=pmc */
int32_t li_rt_preg_get(int32_t table, int32_t idx);
void li_rt_preg_set(int32_t table, int32_t idx, int32_t val);
void li_rt_preg_clear(void);
const char* li_rt_type_src_get(int idx);
void li_rt_set_args(int argc, char** argv);
int li_rt_argc(void);
const char* li_rt_argv(int index);
/* Whole-file read into a NUL-terminated malloc buffer (NULL on failure).
 * Self-hosted compiler seam: the Li lexer reads source through this. */
const char* li_rt_read_file(const char* path);
void li_parallel_for_i64(long long start, long long end, void (*body)(long long), int team_size);
void li_omp_parallel_for_i64(long long start, long long end, void (*body)(long long));
int32_t li_rt_floor_div_i32(int32_t a, int32_t b);
int32_t li_rt_pow_i32(int32_t base, int32_t exp);
double li_rt_sqrt(double x);
double li_rt_sin(double x);
double li_rt_cos(double x);
double li_rt_atan2(double y, double x);
double li_rt_exp(double x);
double li_rt_log(double x);
double li_rt_hypot(double x, double y);
double li_rt_expm1(double x);
double li_rt_log1p(double x);
void li_rt_print_f64(double v);
void li_rt_volatile_sink_f64(double v);

/* HTTP routing + config (li_rt_httpd.c — M1 validate-config / routing tests). */
int32_t li_rt_str_byte_at(const char* s, int32_t i);
int32_t li_rt_str_prefix_is_get(const char* s);
int32_t li_rt_http_parse_request_len_tag(const char* s, int32_t max_header_block, int32_t max_body);
int32_t li_rt_str_eq(const char* a, const char* b);
int32_t li_rt_str_prefix(const char* s, const char* prefix);
int32_t li_rt_path_exact(const char* path, const char* want);
int32_t li_rt_path_prefix(const char* path, const char* prefix);
int32_t li_rt_match_route_fixture(const char* method, const char* path);
int32_t li_rt_httpd_load_config(const char* path);
int32_t li_rt_httpd_explain_config(const char* path);
int32_t li_rt_httpd_last_error_kind(void);
const char* li_rt_httpd_last_error_message(void);
int32_t li_rt_httpd_route_count(void);
int32_t li_rt_httpd_route_key_valid(const char* key);
int32_t li_rt_httpd_serve_once(int32_t port);
int32_t li_rt_httpd_serve_routed_once(int32_t port);
int32_t li_rt_str_len(const char* s);
int32_t li_rt_str_char_at(const char* s, int32_t i);
int32_t li_rt_httpd_load_routing_fixture(void);
int32_t li_rt_httpd_load_m15_agent_fixture(void);
int32_t li_rt_httpd_load_m15_leak_censor_fixture(void);
int32_t li_rt_httpd_load_m15_tls_le_fixture(void);
int32_t li_rt_httpd_load_m15_tls_dev_fixture(void);
int32_t li_rt_httpd_match_route(const char* method, const char* path);
int32_t li_rt_httpd_route_action_kind(int32_t route_id);
int32_t li_rt_httpd_parse_duration_sec(const char* raw);
int32_t li_rt_httpd_m15_stream_idle_sec(void);
int32_t li_rt_httpd_m15_stream_max_sec(void);
int32_t li_rt_httpd_m15_concurrent_streams(void);
int32_t li_rt_httpd_route_requires_traceparent(int32_t route_id);
int32_t li_rt_httpd_is_sse_content_type(const char* ctype);
int32_t li_rt_httpd_traceparent_ok(const char* buf, int32_t hdr_end);
int32_t li_rt_httpd_traceparent_selftest(void);
int32_t li_rt_httpd_leak_censor_enabled(void);
int32_t li_rt_httpd_leak_censor_deny_path_count(void);
int32_t li_rt_httpd_leak_censor_pattern_openai(void);
int32_t li_rt_httpd_leak_censor_pattern_jwt(void);
int32_t li_rt_httpd_leak_censor_pattern_pem(void);
int32_t li_rt_httpd_leak_scrub(const char* data, int32_t len, intptr_t out_buf, int32_t out_cap);
int32_t li_rt_httpd_leak_scrub_hit_count(void);
int32_t li_rt_httpd_leak_scrub_selftest(void);
int32_t li_rt_httpd_tls_enabled(void);
int32_t li_rt_httpd_tls_mode(void);
int32_t li_rt_httpd_tls_le_domain_count(void);
int32_t li_rt_httpd_tls_renew_before_days(void);
int32_t li_rt_httpd_tls_self_signed_dev(void);
const char* li_rt_httpd_tls_le_email(void);
int32_t li_rt_httpd_tls_selftest(void);
int32_t li_rt_httpd_m2_enabled(void);
int32_t li_rt_httpd_m2_tls_terminate(void);
int32_t li_rt_httpd_m2_http2_enabled(void);
int32_t li_rt_httpd_m2_http2_max_streams(void);
int32_t li_rt_httpd_m2_queue_max_depth(void);
int32_t li_rt_httpd_m2_queue_retry_after_sec(void);
int32_t li_rt_httpd_m2_cb_error_threshold(void);
int32_t li_rt_httpd_m2_webhook_allow_count(void);
int32_t li_rt_httpd_route_requires_websocket(int32_t route_id);
int32_t li_rt_httpd_m2_webhook_url_allowed(const char* url);
int32_t li_rt_httpd_m2_selftest(void);
int32_t li_rt_httpd_m3_enabled(void);
int32_t li_rt_httpd_m3_l4_enabled(void);
int32_t li_rt_httpd_m3_l4_listen_port(void);
int32_t li_rt_httpd_m3_l4_upstream_port(void);
int32_t li_rt_httpd_m3_l4_max_connections(void);
int32_t li_rt_httpd_m3_token_budget_enabled(void);
int32_t li_rt_httpd_m3_token_budget_max(void);
int32_t li_rt_httpd_m3_token_budget_check(const char* header_val);
int32_t li_rt_httpd_m3_selftest(void);

void li_async_frame_enter(void);
void li_async_frame_leave(void);
int32_t li_async_await_i32(int32_t pending);
int32_t li_async_poll(uint32_t slot);
int32_t li_async_reactor_register_i(int32_t fd, int32_t slot);
int32_t li_async_reactor_selftest_i(void);
int32_t tcp_echo_epoll_once_i(int32_t port);

/* Net trusted seam (li_rt_net.c) — syscalls + I/O buffers; HTTP in Li packages. */
int32_t net_ping(void);
int32_t tcp_listen(int32_t port);
int32_t tcp_accept(int32_t listen_fd);
int32_t tcp_send(int32_t conn_fd, const char* data);
int32_t tcp_send_n(int32_t conn_fd, const char* data, int32_t len);
const char* tcp_recv(int32_t conn_fd, int32_t max_bytes);
void tcp_close(int32_t fd);
void tcp_tune_client(int32_t fd);
int32_t bytes_len(const char* b);
const char* bytes_slice(const char* b, int32_t off, int32_t n);
const char* bytes_append(const char* a, const char* b);
int32_t bytes_byte_at(const char* b, int32_t off);
const char* bytes_push_byte(const char* buf, int32_t byte);
int32_t net_byte_at(const char* b, int32_t off);
int32_t net_atoi(const char* s);

intptr_t tcp_recv_i(int32_t conn, int32_t max);
int32_t tcp_send_i(int32_t conn, intptr_t data);
intptr_t li_rt_argv_i(int32_t index);
int32_t bytes_len_i(intptr_t b);
intptr_t bytes_slice_i(intptr_t b, int32_t off, int32_t n);
intptr_t bytes_append_i(intptr_t a, intptr_t b);
int32_t net_byte_at_i(intptr_t b, int32_t off);
int32_t httpd_parse_port_i(intptr_t s);

int32_t net_set_nonblock(int32_t fd);
int32_t net_tcp_ack_now(int32_t fd);
int32_t tcp_accept_nb(int32_t listen_fd);
int32_t tcp_recv_slot(int32_t conn, int32_t slot, int32_t max_bytes);
int32_t tcp_send_buf(int32_t conn, intptr_t data, int32_t off, int32_t n);
int32_t tcp_send_coalesce_i(int32_t conn, intptr_t a, int32_t la, intptr_t b, int32_t lb);
int32_t net_buf_len(int32_t slot);
intptr_t net_slot_buf_ptr(int32_t slot);
intptr_t httpd_slot_hdr_i(int32_t slot);
int32_t net_slot_consume(int32_t slot, int32_t n);
int32_t httpd_prepare_root_i(intptr_t root);
int32_t httpd_cache_ready_i(void);
intptr_t httpd_cached_body_i(void);
int32_t httpd_cached_sz_i(void);
int32_t httpd_reply_cached_index_i(int32_t conn, int32_t slot, int32_t keep_alive);
int32_t httpd_drain_slot_i(int32_t conn, int32_t slot);
int32_t httpd_epoll_serve_i(int32_t port, intptr_t root);
int32_t httpd_set_proxy_upstream_i(int32_t host, int32_t port, int32_t proxy_all);
int32_t httpd_set_proxy_upstream_port_i(int32_t port, int32_t proxy_all);
int32_t httpd_set_upstream_ports_csv_i(intptr_t csv, int32_t proxy_all);
int32_t httpd_set_lb_mode_i(int32_t mode);
int32_t httpd_lb_mode_from_arg_i(intptr_t s);
int32_t httpd_mark_upstream_peer_down_i(int32_t port);
int32_t httpd_add_upstream_peer_i(int32_t port);
void httpd_clear_upstream_peers_i(void);
int32_t httpd_tick_active_health_probes_i(void);
int32_t httpd_tick_sse_stream_idle_i(int32_t epfd);
int32_t httpd_sse_idle_epoll_timeout_ms_i(void);
int32_t epoll_wait_tagged_timeout_ms_i(int32_t epfd, intptr_t events, int32_t max_events,
                                        int32_t timeout_ms);
int32_t httpd_load_runtime_config_i(intptr_t path);
int32_t httpd_tls_enabled_i(void);
int32_t httpd_tls_handshake_slot_i(int32_t slot, int32_t fd);
int32_t httpd_tls_slot_h2_i(int32_t slot);
int32_t httpd_h2_serve_slot_i(int32_t epfd, int32_t slot);
void httpd_client_force_close_i(int32_t epfd, int32_t slot);
int32_t httpd_fork_workers_i(void);
int32_t httpd_config_workers_i(void);
int32_t httpd_config_listen_port_i(void);
intptr_t httpd_config_doc_root_i(void);
intptr_t net_lit_loopback_i(void);
int32_t httpd_slot_alloc(int32_t fd);
int32_t httpd_slot_find_fd(int32_t fd);
void httpd_slot_free(int32_t slot);

int32_t epoll_create1_i(void);
int32_t epoll_ctl_add_i(int32_t epfd, int32_t fd);
int32_t epoll_ctl_add_listen_i(int32_t epfd, int32_t fd);
int32_t epoll_ctl_del_i(int32_t epfd, int32_t fd);
int32_t epoll_wait_events_i(int32_t epfd, intptr_t events, int32_t max_events);
int32_t net_events_fd(intptr_t events, int32_t index);
int32_t net_events_revents(intptr_t events, int32_t index);
int32_t net_epoll_readable(int32_t revents);
int32_t net_epoll_hangup(int32_t revents);
int32_t net_fill_not_found_i(intptr_t p);

int32_t net_open_readonly_i(intptr_t path);
int32_t net_fstat_size(int32_t fd);
int32_t net_read_fd(int32_t fd, intptr_t buf, int32_t max_bytes);
int32_t net_close_fd(int32_t fd);
int32_t net_sendfile_fd(int32_t conn, int32_t file_fd, int32_t file_size);

intptr_t net_buf_alloc(int32_t size);
void net_buf_free(intptr_t p);
int32_t net_buf_fill_i(intptr_t dst, intptr_t src, int32_t off, int32_t n);
int32_t httpd_write_response_hdr_i(intptr_t buf, int32_t cap, int32_t status, int32_t body_len,
                                   int32_t keep_alive);

intptr_t str_cat2_i(intptr_t a, intptr_t b);
intptr_t net_lit_index_html_i(void);
int32_t net_diag(int32_t tag);

/* Logging seam (li_rt_log.c — packages/li-log M1). */
void li_rt_log_set_dir(const char* dir);
int32_t li_rt_log_redact_line(const char* in, char* out, int32_t cap);
void li_rt_log_access_line(const char* ts, const char* method, const char* path, int32_t status,
                           int32_t bytes_out);
int32_t li_rt_log_reopen(void);
const char* li_rt_log_redact(const char* in);
int32_t li_rt_log_redact_ok(const char* in);

#ifdef __cplusplus
}
#endif
int32_t li_rt_studio_profile_from_name(const char* name);
int32_t li_rt_studio_parse_toml_profile_line(const char* line);
int32_t li_rt_studio_timeline_playing(void);
int32_t li_rt_studio_timeline_toggle_play(void);
int32_t li_rt_studio_timeline_tick_frame(void);
float li_rt_studio_timeline_playhead_pct(void);
int32_t li_rt_studio_timeline_reset_mock(void);
int32_t li_rt_studio_viewport_error_kind(void);
int32_t li_rt_studio_viewport_error_set_mock(int32_t kind);
int32_t li_rt_studio_viewport_error_retry(void);
int32_t li_rt_studio_mcp_tool_from_name(const char* name);
const char* li_rt_studio_mcp_tool_name(int32_t tool_id);
int32_t li_rt_lig_host_present_active(void);
float li_rt_lig_host_present_dt_ms(void);
int32_t li_rt_lig_host_native_pixels(void);
int32_t li_rt_lig_wgpu_swapchain_create(int32_t viewport_w, int32_t viewport_h);
int32_t li_rt_lig_wgpu_present_frame(int32_t swapchain_ok);
int32_t li_rt_studio_shell_input_pointer_down(void);
float li_rt_studio_shell_input_pointer_x(void);
float li_rt_studio_shell_input_pointer_y(void);
int32_t li_rt_studio_shell_input_key_escape(void);
int32_t li_rt_studio_shell_input_key_cmd_k(void);
int32_t li_rt_studio_shell_input_key_digit(void);
int32_t li_rt_studio_host_present_tick(int32_t viewport_w, int32_t viewport_h);
int32_t li_rt_studio_demo_profile_from_env(void);


/* PH-HW HW-0: lig device layer. */
int32_t li_rt_lig_device_kind(void);
int32_t li_rt_lig_backend_available(int32_t backend_id);
int32_t li_rt_lig_backend_select_auto(void);
const char* li_rt_lig_capability_json(void);
int32_t li_rt_lig_parse_toml_backend_line(const char* line);
int32_t li_rt_lig_present_surface_ok(void);


/* PH-HW HW-0: lig device layer. */
int32_t li_rt_lig_device_kind(void);
int32_t li_rt_lig_backend_available(int32_t backend_id);
int32_t li_rt_lig_backend_select_auto(void);
const char* li_rt_lig_capability_json(void);
int32_t li_rt_lig_parse_toml_backend_line(const char* line);
int32_t li_rt_lig_present_surface_ok(void);

/* PH-GD-2: li-world text save/load seam (in-memory buffer; no filesystem I/O). */
int32_t li_rt_world_format_version(void);
const char* li_rt_world_serialize_slot(int32_t name_slot, int32_t tick, int32_t entity_count);
int32_t li_rt_world_parse_line(const char* line);
int32_t li_rt_world_parsed_name_slot(void);
int32_t li_rt_world_parsed_tick(void);
int32_t li_rt_world_parsed_entity_count(void);
int32_t li_rt_world_snapshot_eq_fields(int32_t an, int32_t at, int32_t ae, int32_t bn, int32_t bt,
                                       int32_t be);
int32_t li_rt_world_roundtrip_fields(int32_t name_slot, int32_t tick, int32_t entity_count);
