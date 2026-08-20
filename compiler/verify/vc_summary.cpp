#include "li/vc_summary.hpp"

namespace li {
namespace {

void count_contracts(const std::vector<Contract>& contracts, VcSummary& out) {
  for (const auto& c : contracts) {
    switch (c.kind) {
      case ContractKind::Requires:
        ++out.requires_count;
        break;
      case ContractKind::Ensures:
        ++out.ensures_count;
        break;
      case ContractKind::ProbEnsures:
        ++out.prob_ensures_count;
        break;
      case ContractKind::Decreases:
        ++out.decreases_count;
        break;
      case ContractKind::Invariant:
        ++out.invariant_count;
        break;
    }
  }
}

void walk_stmts(const std::vector<Stmt>& stmts, VcSummary& out) {
  for (const auto& s : stmts) {
    count_contracts(s.par_contracts, out);
    count_contracts(s.for_contracts, out);
    walk_stmts(s.then_body, out);
    if (s.else_body) {
      walk_stmts(*s.else_body, out);
    }
    walk_stmts(s.while_body, out);
    walk_stmts(s.for_body, out);
    walk_stmts(s.par_body, out);
  }
}

}  // namespace

VcSummary summarize_vcs(const Module& module) {
  VcSummary out;
  out.proc_count = module.procs.size();
  for (const auto& proc : module.procs) {
    count_contracts(proc.contracts, out);
    walk_stmts(proc.body, out);
  }
  for (const auto& thm : module.theorems) {
    if (thm.is_axiom) {
      ++out.axiom_count;
    } else if (thm.is_lemma) {
      ++out.lemma_count;
    } else {
      ++out.theorem_count;
    }
  }
  return out;
}

}  // namespace li
