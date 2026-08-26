#pragma once

#include "li/diagnostics.hpp"
#include "li/token.hpp"

#include <string>
#include <vector>

namespace li {

class Lexer {
 public:
  Lexer(std::string source, std::string file);

  bool tokenize(DiagnosticBag& diags);
  const std::vector<Token>& tokens() const { return tokens_; }

 private:
  bool at_end() const;
  char peek() const;
  char advance();
  void skip_whitespace_inline();
  bool lex_number(Token& out, bool is_float_start = false);
  bool lex_binary_literal(Token& out);
  void lex_literal_suffix(Token& out, bool is_float);
  bool lex_ident_or_keyword(Token& out);
  bool lex_string(Token& out);
  void push_token(Token t);
  bool process_line_begin(std::size_t line_start, DiagnosticBag& diags);
  TokenKind keyword_kind(std::string_view text) const;

  std::string source_;
  std::string file_;
  std::size_t pos_ = 0;
  std::size_t line_ = 1;
  std::size_t column_ = 1;
  std::vector<Token> tokens_;
  std::vector<std::size_t> indent_stack_{0};
  bool at_line_start_ = true;
  bool pending_indent_check_ = false;
  bool body_mode_ = false;
};

}  // namespace li
