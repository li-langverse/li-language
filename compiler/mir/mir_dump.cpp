#include "li/mir_dump.hpp"

#include <cinttypes>
#include <cstdio>
#include <sstream>

namespace li {
namespace {

std::string esc(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (const char c : s) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      case ' ':
        out += "\\x20";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

// Shortest round-trip double, deterministic across runs/compilers.
std::string fmt_f64(double v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.17g", v);
  return std::string(buf);
}

struct Dumper {
  std::ostringstream out;

  void nl() { out << '\n'; }
  void sp() { out << ' '; }
  void field(const std::string& s) { out << esc(s); }
  void field(std::int64_t v) { out << v; }
  void field(double v) { out << fmt_f64(v); }
  void field(bool v) { out << (v ? '1' : '0'); }
};

void dump_param(const MirParam& p, const char* tag, Dumper& d) {
  d.out << tag;
  d.sp();
  d.field(p.name);
  d.sp();
  d.field(p.is_float);
  d.sp();
  d.field(p.is_string);
  d.sp();
  d.field(p.is_i64);
  d.sp();
  d.field(p.is_simd_f64);
  d.sp();
  d.field(p.simd_lanes);
  d.sp();
  d.field(p.fixed_array_elems);
  d.sp();
  d.field(p.is_matrix);
  d.sp();
  d.field(p.matrix_cols);
  d.sp();
  d.field(p.is_var);
  d.nl();
}

void dump_decorator(const MirDecorator& dec, Dumper& d) {
  d.out << "DEC";
  d.sp();
  d.field(dec.name);
  d.sp();
  d.field(dec.lanes);
  d.sp();
  d.field(dec.vectorized);
  d.sp();
  d.field(dec.parallel);
  d.sp();
  d.field(dec.disjoint_proven);
  d.nl();
}

void dump_arg(const MirArg& a, Dumper& d) {
  d.out << "ARG";
  d.sp();
  d.field(a.is_literal);
  d.sp();
  d.field(a.int_value);
  d.sp();
  d.field(a.is_float_literal);
  d.sp();
  d.field(a.float_value);
  d.sp();
  d.field(a.ident);
  d.sp();
  d.field(a.is_string);
  d.sp();
  d.field(a.str_value);
  d.sp();
  d.field(a.is_array_ident);
  d.sp();
  d.field(a.is_var_ref);
  d.nl();
}

void dump_insn(const MirInsn& i, Dumper& d) {
  d.out << "INS";
  d.sp();
  d.field(static_cast<std::int64_t>(i.op));
  d.sp();
  d.field(i.int_value);
  d.sp();
  d.field(i.float_value);
  d.sp();
  d.field(i.ident);
  d.sp();
  d.field(i.str_value);
  d.sp();
  d.field(i.callee);
  d.sp();
  d.field(i.lhs_ident);
  d.sp();
  d.field(i.rhs_ident);
  d.sp();
  d.field(i.label);
  d.sp();
  d.field(static_cast<std::int64_t>(i.bin_op));
  d.sp();
  d.field(i.ret_is_float);
  d.sp();
  d.field(i.ret_is_i64);
  d.sp();
  d.field(i.index_is_literal);
  d.sp();
  d.field(i.index_ident);
  d.sp();
  d.field(i.use_loaded_int);
  d.sp();
  d.field(i.rhs_is_literal);
  d.sp();
  d.field(i.rhs_int);
  d.sp();
  d.field(i.rhs_is_string);
  d.sp();
  d.field(i.lhs_is_literal);
  d.sp();
  d.field(i.lhs_int);
  d.sp();
  d.field(i.is_i64);
  d.sp();
  d.field(i.array_is_float);
  d.sp();
  d.field(i.array_is_matrix);
  d.sp();
  d.field(i.array_broadcast_lhs_len1);
  d.sp();
  d.field(i.array_broadcast_rhs_len1);
  d.sp();
  d.field(i.simd_lanes);
  d.nl();
  for (const auto& a : i.args) {
    dump_arg(a, d);
  }
  for (const auto& p : i.object_layout) {
    dump_param(p, "OBJ", d);
  }
}

}  // namespace

std::string dump_mir_module(const MirModule& m) {
  Dumper d;
  d.out << "MIR";
  d.sp();
  d.field(static_cast<std::int64_t>(m.functions.size()));
  d.sp();
  d.field(m.uses_openmp);
  d.sp();
  d.field(m.uses_async);
  d.sp();
  d.field(m.needs_rt_httpd);
  d.sp();
  d.field(m.needs_rt_net);
  d.sp();
  d.field(m.needs_rt_log);
  d.sp();
  d.field(m.fp_numerically_stable);
  d.nl();
  for (const auto& fn : m.functions) {
    d.out << "FN";
    d.sp();
    d.field(fn.name);
    d.sp();
    d.field(fn.returns_float);
    d.sp();
    d.field(fn.returns_i64);
    d.sp();
    d.field(fn.returns_void);
    d.sp();
    d.field(fn.returns_object);
    d.sp();
    d.field(fn.is_extern);
    d.sp();
    d.field(fn.is_async);
    d.sp();
    d.field(fn.no_vectorize);
    d.nl();
    for (const auto& dec : fn.decorators) {
      dump_decorator(dec, d);
    }
    for (const auto& p : fn.params) {
      dump_param(p, "PARAM", d);
    }
    for (const auto& p : fn.return_object_layout) {
      dump_param(p, "RETPARAM", d);
    }
    for (const auto& ins : fn.body) {
      dump_insn(ins, d);
    }
  }
  return d.out.str();
}

}  // namespace li
