#include "li/mir_runtime_link.hpp"

namespace li {
namespace {

bool starts_with(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

void note_one(std::string_view callee, MirModule& mir) {
  if (starts_with(callee, "li_log_") || starts_with(callee, "li_rt_log_")) {
    mir.needs_rt_log = true;
  }
  if (starts_with(callee, "httpd_") || starts_with(callee, "li_rt_httpd_") ||
      starts_with(callee, "proxy_") || starts_with(callee, "li_rt_proxy_")) {
    mir.needs_rt_httpd = true;
  }
  if (starts_with(callee, "net_") || starts_with(callee, "tcp_") || starts_with(callee, "epoll_") ||
      starts_with(callee, "hdr_") || starts_with(callee, "buf_") || starts_with(callee, "bytes_") ||
      callee == "path_ends_with_conf" || callee == "net_ping" ||
      starts_with(callee, "li_rt_net") || starts_with(callee, "li_async_") ||
      starts_with(callee, "tcp_echo_")) {
    mir.needs_rt_net = true;
  }
}

}  // namespace

void mir_note_runtime_callee(std::string_view callee, MirModule& mir) {
  note_one(callee, mir);
}

void mir_finalize_runtime_link_needs(MirModule& mir) {
  if (mir.needs_rt_httpd) {
    mir.needs_rt_net = true;
  }
  // li_rt_net.c (httpd access log, proxy) calls li_rt_log_* in li_rt_log.c.
  if (mir.needs_rt_net) {
    mir.needs_rt_log = true;
  }
}

void mir_collect_runtime_link_needs(const MirModule& mir, MirModule& out_flags) {
  for (const auto& fn : mir.functions) {
    for (const auto& ins : fn.body) {
      if (ins.op == MirOp::CallExtern && !ins.callee.empty()) {
        mir_note_runtime_callee(ins.callee, out_flags);
      }
      if (ins.op == MirOp::AsyncAwait || ins.op == MirOp::AsyncFrameEnter ||
          ins.op == MirOp::AsyncFrameLeave) {
        out_flags.needs_rt_net = true;
      }
    }
  }
  mir_finalize_runtime_link_needs(out_flags);
}

}  // namespace li
