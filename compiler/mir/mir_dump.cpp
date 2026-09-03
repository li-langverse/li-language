#include "li/mir_dump.hpp"

#include <cstdio>
#include <sstream>

namespace li {
namespace {

std::string esc(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (const char c : s) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case ' ':  out += "\\x20"; break;
      default:   out += c; break;
    }
  }
  return out;
}

std::string fmt_f64(double v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.17g", v);
  return std::string(buf);
}

struct D {
  std::ostringstream o;
  void nl() { o << '\n'; }
  void sp() { o << ' '; }
  void f(const std::string& s) { o << esc(s); }
  void f(std::int64_t v) { o << v; }
  void f(double v) { o << fmt_f64(v); }
  void f(bool v) { o << (v ? '1' : '0'); }
};

// Field order and counts are part of the MIR text ABI: they mirror the
// self-hosted walker's emitters in bootstrap/lic/main.li exactly.
void dump_param(const MirParam& p, const char* tag, D& d) {
  d.o << tag;
  d.sp(); d.f(p.name);
  d.sp(); d.f(p.is_float);
  d.sp(); d.f(p.is_string);
  d.sp(); d.f(p.is_i64);
  d.sp(); d.f(p.is_simd_f64);
  d.sp(); d.f(p.simd_lanes);
  d.sp(); d.f(p.fixed_array_elems);
  d.sp(); d.f(p.is_matrix);
  d.sp(); d.f(p.matrix_cols);
  d.sp(); d.f(p.is_var);
  d.nl();
}

void dump_decorator(const MirDecorator& dec, D& d) {
  // Walker field order: name, lanes, vectorized, parallel, disjoint.
  d.o << "DEC";
  d.sp(); d.f(dec.name);
  d.sp(); d.f(dec.lanes);
  d.sp(); d.f(dec.vectorized);
  d.sp(); d.f(dec.parallel);
  d.sp(); d.f(dec.disjoint_proven);
  d.nl();
}

void dump_arg(const MirArg& a, D& d) {
  d.o << "ARG";
  d.sp(); d.f(a.is_literal);
  d.sp(); d.f(a.int_value);
  d.sp(); d.f(a.is_float_literal);
  d.sp(); d.f(a.float_value);
  d.sp(); d.f(a.ident);
  d.sp(); d.f(a.is_string);
  d.sp(); d.f(a.str_value);
  d.sp(); d.f(a.is_array_ident);
  d.sp(); d.f(a.is_var_ref);
  d.nl();
}

void dump_insn(const MirInsn& i, D& d) {
  d.o << "INS";
  d.sp();
  // The walker emits 1D array loads as op 11 (ArrayLoadInt) for BOTH int and
  // float arrays; ArrayLoadFloat (13) exists only in this enum, so float
  // loads must dump as 11 to match. Stores keep their distinct ops (10/12).
  const std::int64_t dump_op =
      i.op == MirOp::ArrayLoadFloat ? 11 : static_cast<std::int64_t>(i.op);
  d.f(dump_op);
  d.sp(); d.f(i.int_value);
  d.sp(); d.f(i.float_value);
  d.sp(); d.f(i.ident);
  d.sp(); d.f(i.str_value);
  d.sp(); d.f(i.callee);
  d.sp(); d.f(i.lhs_ident);
  d.sp(); d.f(i.rhs_ident);
  d.sp(); d.f(i.label);
  d.sp(); d.f(static_cast<std::int64_t>(i.bin_op));
  d.sp(); d.f(i.ret_is_float);
  d.sp(); d.f(i.ret_is_i64);
  d.sp(); d.f(i.index_is_literal);
  d.sp(); d.f(i.index_ident);
  d.sp(); d.f(i.use_loaded_int);
  d.sp(); d.f(i.rhs_is_literal);
  d.sp(); d.f(i.rhs_int);
  d.sp(); d.f(i.rhs_is_string);
  d.sp(); d.f(i.lhs_is_literal);
  d.sp(); d.f(i.lhs_int);
  d.sp(); d.f(i.is_i64);
  d.sp(); d.f(i.array_is_float);
  d.sp(); d.f(i.array_is_matrix);
  d.sp(); d.f(i.array_broadcast_lhs_len1);
  d.sp(); d.f(i.array_broadcast_rhs_len1);
  d.sp(); d.f(i.simd_lanes);
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
  D d;
  d.o << "MIR";
  d.sp(); d.f(static_cast<std::int64_t>(m.functions.size()));
  d.sp(); d.f(m.uses_openmp);
  d.sp(); d.f(m.uses_async);
  d.sp(); d.f(m.needs_rt_httpd);
  d.sp(); d.f(m.needs_rt_net);
  d.sp(); d.f(m.needs_rt_log);
  d.sp(); d.f(m.fp_numerically_stable);
  d.nl();
  for (const auto& fn : m.functions) {
    d.o << "FN";
    d.sp(); d.f(fn.name);
    d.sp(); d.f(fn.returns_float);
    d.sp(); d.f(fn.returns_i64);
    d.sp(); d.f(fn.returns_void);
    d.sp(); d.f(fn.returns_object);
    d.sp(); d.f(fn.is_extern);
    d.sp(); d.f(fn.is_async);
    d.sp(); d.f(fn.no_vectorize);
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
  return d.o.str();
}

}  // namespace li
