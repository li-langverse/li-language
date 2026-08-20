#include "li/vc_emit.hpp"

#include <fstream>

namespace li {

bool write_vcs_json(const Module& module, const std::string& path, std::string* err) {
  const VcSummary vc = summarize_vcs(module);
  std::ofstream out(path);
  if (!out) {
    if (err) {
      *err = "cannot open " + path;
    }
    return false;
  }
  out << "{\n"
      << "  \"procs\": " << vc.proc_count << ",\n"
      << "  \"requires\": " << vc.requires_count << ",\n"
      << "  \"ensures\": " << vc.ensures_count << ",\n"
      << "  \"prob_ensures\": " << vc.prob_ensures_count << ",\n"
      << "  \"decreases\": " << vc.decreases_count << ",\n"
      << "  \"invariant\": " << vc.invariant_count << ",\n"
      << "  \"ensures_witnessed\": " << vc.ensures_witnessed << ",\n"
      << "  \"mir_return_linked\": " << vc.mir_return_linked << ",\n"
      << "  \"axioms\": " << vc.axiom_count << ",\n"
      << "  \"theorems\": " << vc.theorem_count << ",\n"
      << "  \"lemmas\": " << vc.lemma_count << "\n"
      << "}\n";
  return true;
}

}  // namespace li
