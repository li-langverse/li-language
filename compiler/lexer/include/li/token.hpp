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
  StringLit,
  KwProc,
  KwType,
  KwObject,
  KwEnum,
  KwVar,
  KwLet,
  KwIf,
  KwElse,
  KwElif,
  KwWhile,
  KwReturn,
  KwRaises,
  KwEcho,
  KwExtern,
  KwTrue,
  KwFalse,
  KwAnd,
  KwOr,
  KwNot,
  KwIs,
  KwRequires,
  KwEnsures,
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
  Slash,
  Le,
  Lt,
  Ge,
  Gt,
  EqEq,
  Ne,
  DotDotLt,
  Pipe,
  Ellipsis,
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
};

const char* token_kind_name(TokenKind kind);

}  // namespace li
