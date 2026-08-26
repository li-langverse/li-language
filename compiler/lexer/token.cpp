#include "li/token.hpp"

namespace li {

const char* token_kind_name(TokenKind kind) {
  switch (kind) {
    case TokenKind::Eof: return "Eof";
    case TokenKind::Newline: return "Newline";
    case TokenKind::Indent: return "Indent";
    case TokenKind::Dedent: return "Dedent";
    case TokenKind::Ident: return "Ident";
    case TokenKind::IntLit: return "IntLit";
    case TokenKind::KwProc: return "proc";
    case TokenKind::KwAxiom: return "axiom";
    case TokenKind::KwTheorem: return "theorem";
    case TokenKind::KwLemma: return "lemma";
    case TokenKind::KwRequires: return "requires";
    case TokenKind::KwEnsures: return "ensures";
    case TokenKind::KwProbEnsures: return "prob_ensures";
    case TokenKind::KwDecreases: return "decreases";
    case TokenKind::KwResult: return "result";
    case TokenKind::At: return "@";
    default: return "Token";
  }
}

}  // namespace li
