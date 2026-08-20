#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace li {

enum class TokenKind {
  Eof,
  Newline,
  Indent,
  Dedent,
  Ident,
  IntLit,
  FloatLit,
  BinaryLit,
  StringLit,
  KwProc,
  KwDef,
  KwType,
  KwAxiom,
  KwTheorem,
  KwLemma,
  KwObject,
  KwPrivate,
  KwPublic,
  KwImport,
  KwEnum,
  KwVar,
  KwLet,
  KwIf,
  KwElse,
  KwElif,
  KwWhile,
  KwFor,
  KwBreak,
  KwContinue,
  KwError,
  KwReturn,
  KwRaises,
  KwExtern,
  KwAsync,
  KwAwait,
  KwTrue,
  KwFalse,
  KwAnd,
  KwOr,
  KwNot,
  KwIs,
  KwRequires,
  KwEnsures,
  KwProbEnsures,
  KwDecreases,
  KwInvariant,
  KwResult,
  KwProtocol,
  KwCallable,
  LParen,
  RParen,
  LBracket,
  RBracket,
  LBrace,
  RBrace,
  Comma,
  Colon,
  Arrow,
  Eq,
  Plus,
  Minus,
  Star,
  StarStar,
  Slash,
  SlashSlash,
  Percent,
  Le,
  Lt,
  Ge,
  Gt,
  EqEq,
  Ne,
  Dot,
  DotDotLt,
  Pipe,
  Ellipsis,
  At,
};

struct Token {
  TokenKind kind = TokenKind::Eof;
  std::string_view text;
  std::size_t start = 0;
  std::size_t end = 0;
  std::size_t line = 1;
  std::size_t column = 1;
  std::int64_t int_value = 0;
  double float_value = 0.0;
  /// Suffix after numeric literal (`f32`, `i32`, `u`, …). Empty = default width.
  std::string lit_suffix;
};

const char* token_kind_name(TokenKind kind);

}  // namespace li
