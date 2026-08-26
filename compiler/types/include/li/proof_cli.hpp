#pragma once

namespace li {

/** Set by `lic build` / `lic verify` from CLI flags only (not env). */
struct ProofCliFlags {
  bool allow_open_vc = false;
  bool no_lean_verify = false;
  bool prob_check = false;
};

inline ProofCliFlags& proof_cli_flags() {
  static ProofCliFlags flags;
  return flags;
}

inline void reset_proof_cli_flags() {
  proof_cli_flags() = ProofCliFlags{};
}

inline bool allow_open_vc() {
  return proof_cli_flags().allow_open_vc;
}

inline bool no_lean_verify() {
  return proof_cli_flags().no_lean_verify;
}

inline bool prob_check() {
  return proof_cli_flags().prob_check;
}

}  // namespace li
