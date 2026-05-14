#include "li/lexer.hpp"

#include <cctype>

namespace li {

Lexer::Lexer(std::string source, std::string file)
    : source_(std::move(source)), file_(std::move(file)) {}

bool Lexer::at_end() const { return pos_ >= source_.size(); }

char Lexer::peek() const { return at_end() ? '\0' : source_[pos_]; }

char Lexer::advance() {
  if (at_end()) {
    return '\0';
  }
  const char c = source_[pos_++];
  if (c == '\n') {
    line_++;
    column_ = 1;
    at_line_start_ = true;
  } else {
    column_++;
    at_line_start_ = false;
  }
  return c;
}

void Lexer::push_token(Token t) { tokens_.push_back(std::move(t)); }

TokenKind Lexer::keyword_kind(std::string_view text) const {
  if (text == "proc") return TokenKind::KwProc;
  if (text == "type") return TokenKind::KwType;
  if (text == "object") return TokenKind::KwObject;
  if (text == "enum") return TokenKind::KwEnum;
  if (text == "var") return TokenKind::KwVar;
  if (text == "let") return TokenKind::KwLet;
  if (text == "if") return TokenKind::KwIf;
  if (text == "else") return TokenKind::KwElse;
  if (text == "elif") return TokenKind::KwElif;
  if (text == "while") return TokenKind::KwWhile;
  if (text == "return") return TokenKind::KwReturn;
  if (text == "raises") return TokenKind::KwRaises;
  if (text == "echo") return TokenKind::KwEcho;
  if (text == "extern") return TokenKind::KwExtern;
  if (text == "true") return TokenKind::KwTrue;
  if (text == "false") return TokenKind::KwFalse;
  if (text == "and") return TokenKind::KwAnd;
  if (text == "or") return TokenKind::KwOr;
  if (text == "not") return TokenKind::KwNot;
  if (text == "is") return TokenKind::KwIs;
  if (text == "requires") return TokenKind::KwRequires;
  if (text == "ensures") return TokenKind::KwEnsures;
  if (text == "decreases") return TokenKind::KwDecreases;
  if (text == "invariant") return TokenKind::KwInvariant;
  if (text == "result") return TokenKind::KwResult;
  if (text == "Protocol") return TokenKind::KwProtocol;
  if (text == "Callable") return TokenKind::KwCallable;
  if (text == "NotRequired") return TokenKind::Ident;
  if (text == "Required") return TokenKind::Ident;
  if (text == "str") return TokenKind::Ident;
  if (text == "typedict") return TokenKind::Ident;
  return TokenKind::Ident;
}

bool Lexer::process_line_begin(std::size_t line_start, DiagnosticBag& diags) {
  std::size_t i = line_start;
  std::size_t spaces = 0;
  while (i < source_.size() && source_[i] == ' ') {
    spaces++;
    i++;
  }
  if (i < source_.size() && source_[i] == '\t') {
    SourceLoc loc{file_, line_, 1, line_start};
    diags.error(loc, "tabs are not allowed for indentation");
    return false;
  }
  if (i < source_.size() && source_[i] == '#') {
    while (i < source_.size() && source_[i] != '\n') {
      i++;
    }
    return true;
  }

  if (!body_mode_) {
    if (spaces == 0 && i < source_.size() && source_[i] == '=') {
      const std::size_t eq_start = i;
      i++;
      if (i >= source_.size() || source_[i] == '\n' || source_[i] == '\r') {
        Token t;
        t.kind = TokenKind::Eq;
        t.line = line_;
        t.column = 1;
        t.start = eq_start;
        t.end = i;
        t.text = std::string_view(source_).substr(eq_start, i - eq_start);
        push_token(t);
        body_mode_ = true;
        indent_stack_ = {0};
        pending_indent_check_ = false;
        pos_ = i;
        column_ = 1;
        at_line_start_ = (i < source_.size() && source_[i] == '\n');
        return true;
      }
    }
    pos_ = i;
    column_ = spaces + 1;
    at_line_start_ = false;
    return true;
  }

  const std::size_t cur = indent_stack_.back();
  if (pending_indent_check_ && spaces <= cur) {
    SourceLoc loc{file_, line_, 1, line_start};
    diags.error(loc, "expected an indented block after ':' (indentation error)");
    return false;
  }
  if (spaces > cur) {
    if (spaces - cur != 2) {
      SourceLoc loc{file_, line_, 1, line_start};
      diags.error(loc, "indentation must increase by 2 spaces");
      return false;
    }
    indent_stack_.push_back(spaces);
    Token t;
    t.kind = TokenKind::Indent;
    t.line = line_;
    t.column = 1;
    t.start = line_start;
    t.end = i;
    push_token(t);
  } else if (spaces < cur) {
    while (indent_stack_.size() > 1 && indent_stack_.back() > spaces) {
      indent_stack_.pop_back();
      Token t;
      t.kind = TokenKind::Dedent;
      t.line = line_;
      t.column = 1;
      t.start = line_start;
      t.end = line_start;
      push_token(t);
    }
    if (indent_stack_.back() != spaces) {
      SourceLoc loc{file_, line_, 1, line_start};
      diags.error(loc, "inconsistent indentation");
      return false;
    }
    if (indent_stack_.size() == 1 && spaces == 0) {
      body_mode_ = false;
    }
  }
  pending_indent_check_ = false;

  if (i < source_.size() && source_[i] == '\n') {
    pos_ = i;
    column_ = spaces + 1;
    at_line_start_ = false;
    return true;
  }

  if (i < source_.size() && source_[i] == '=') {
    std::size_t j = i + 1;
    while (j < source_.size() && source_[j] == ' ') {
      j++;
    }
    if (j >= source_.size() || source_[j] == '\n' || source_[j] == '\r') {
      Token t;
      t.kind = TokenKind::Eq;
      t.line = line_;
      t.column = spaces + 1;
      t.start = i;
      t.end = i + 1;
      t.text = std::string_view(source_).substr(i, 1);
      push_token(t);
      pos_ = j;
      column_ = spaces + 1;
      at_line_start_ = (j < source_.size() && source_[j] == '\n');
      return true;
    }
  }

  pos_ = i;
  column_ = spaces + 1;
  at_line_start_ = false;
  return true;
}

void Lexer::skip_whitespace_inline() {
  while (!at_end()) {
    const char c = peek();
    if (c == ' ' || c == '\r') {
      advance();
    } else if (c == '#') {
      while (!at_end() && peek() != '\n') {
        advance();
      }
    } else {
      break;
    }
  }
}

bool Lexer::lex_number(Token& out, bool is_float_start) {
  const std::size_t start = pos_;
  const std::size_t sl = line_;
  const std::size_t sc = column_;
  std::int64_t int_value = 0;
  bool is_float = is_float_start;
  while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
    int_value = int_value * 10 + (peek() - '0');
    advance();
  }
  if (!is_float && peek() == '.') {
    const char next = pos_ + 1 < source_.size() ? source_[pos_ + 1] : '\0';
    if (std::isdigit(static_cast<unsigned char>(next))) {
      is_float = true;
      advance();
      while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
      }
    }
  }
  if (peek() == 'e' || peek() == 'E') {
    is_float = true;
    advance();
    if (peek() == '+' || peek() == '-') {
      advance();
    }
    while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
      advance();
    }
  }
  out.start = start;
  out.end = pos_;
  out.line = sl;
  out.column = sc;
  out.text = std::string_view(source_).substr(start, pos_ - start);
  if (is_float) {
    out.kind = TokenKind::FloatLit;
    out.float_value = std::stod(std::string(out.text));
  } else {
    out.kind = TokenKind::IntLit;
    out.int_value = int_value;
  }
  return true;
}

bool Lexer::lex_ident_or_keyword(Token& out) {
  const std::size_t start = pos_;
  const std::size_t sl = line_;
  const std::size_t sc = column_;
  while (!at_end()) {
    const char c = peek();
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
      advance();
    } else {
      break;
    }
  }
  const auto text = std::string_view(source_).substr(start, pos_ - start);
  out.kind = keyword_kind(text);
  out.text = text;
  out.start = start;
  out.end = pos_;
  out.line = sl;
  out.column = sc;
  return true;
}

bool Lexer::lex_string(Token& out) {
  const std::size_t start = pos_;
  const std::size_t sl = line_;
  const std::size_t sc = column_;
  advance();
  while (!at_end() && peek() != '"') {
    advance();
  }
  if (!at_end()) {
    advance();
  }
  out.kind = TokenKind::StringLit;
  out.text = std::string_view(source_).substr(start, pos_ - start);
  out.start = start;
  out.end = pos_;
  out.line = sl;
  out.column = sc;
  return true;
}

bool Lexer::tokenize(DiagnosticBag& diags) {
  pos_ = 0;
  line_ = 1;
  column_ = 1;
  at_line_start_ = true;
  tokens_.clear();
  indent_stack_ = {0};
  pending_indent_check_ = false;
  body_mode_ = false;

  while (!at_end()) {
    if (at_line_start_) {
      const std::size_t line_start = pos_;
      if (!process_line_begin(line_start, diags)) {
        return false;
      }
      if (at_end()) {
        break;
      }
      if (pos_ < source_.size() && source_[pos_] == '\n') {
        advance();
        Token nl;
        nl.kind = TokenKind::Newline;
        nl.line = line_ - 1;
        push_token(nl);
        continue;
      }
    }

    skip_whitespace_inline();
    if (at_end()) {
      break;
    }

    const std::size_t start = pos_;
    const std::size_t sl = line_;
    const std::size_t sc = column_;
    const char c = advance();

    if (c == '\n') {
      Token nl;
      nl.kind = TokenKind::Newline;
      nl.start = start;
      nl.end = pos_;
      nl.line = sl;
      push_token(nl);
      continue;
    }

    Token t;
    t.start = start;
    t.line = sl;
    t.column = sc;

    auto single = [&](TokenKind kind) {
      t.kind = kind;
      t.end = pos_;
      t.text = std::string_view(source_).substr(start, pos_ - start);
      push_token(t);
    };

    switch (c) {
      case '(': single(TokenKind::LParen); continue;
      case ')': single(TokenKind::RParen); continue;
      case '[': single(TokenKind::LBracket); continue;
      case ']': single(TokenKind::RBracket); continue;
      case '{': single(TokenKind::LBrace); continue;
      case '}': single(TokenKind::RBrace); continue;
      case ',': single(TokenKind::Comma); continue;
      case ':':
        if (body_mode_) {
          std::size_t j = pos_;
          while (j < source_.size() && (source_[j] == ' ' || source_[j] == '\t')) {
            j++;
          }
          if (j >= source_.size() || source_[j] == '\n' || source_[j] == '#') {
            pending_indent_check_ = true;
          }
        }
        single(TokenKind::Colon);
        continue;
      case '.':
        if (peek() == '.') {
          advance();
          if (peek() == '.') {
            advance();
            single(TokenKind::Ellipsis);
          } else if (peek() == '<') {
            advance();
            single(TokenKind::DotDotLt);
          } else {
            SourceLoc loc{file_, sl, sc, start};
            diags.error(loc, "expected '..<' range operator or '...'");
            return false;
          }
        } else {
          SourceLoc loc{file_, sl, sc, start};
          diags.error(loc, "unexpected character '.'");
          return false;
        }
        continue;
      case '|': single(TokenKind::Pipe); continue;
      case '+':
      case '-':
        if (peek() == '>') {
          advance();
          t.kind = TokenKind::Arrow;
          t.end = pos_;
          t.text = std::string_view(source_).substr(start, pos_ - start);
          push_token(t);
        } else {
          single(TokenKind::Minus);
        }
        continue;
      case '*': single(TokenKind::Star); continue;
      case '/': single(TokenKind::Slash); continue;
      case '=':
        if (peek() == '=') {
          advance();
          single(TokenKind::EqEq);
        } else if (!body_mode_ && at_line_start_ && sc == 1) {
          single(TokenKind::Eq);
        } else {
          single(TokenKind::Eq);
        }
        continue;
      case '<':
        if (peek() == '=') {
          advance();
          single(TokenKind::Le);
        } else {
          single(TokenKind::Lt);
        }
        continue;
      case '>':
        if (peek() == '=') {
          advance();
          single(TokenKind::Ge);
        } else {
          single(TokenKind::Gt);
        }
        continue;
      case '!':
        if (peek() == '=') {
          advance();
          single(TokenKind::Ne);
        } else {
          SourceLoc loc{file_, sl, sc, start};
          diags.error(loc, "unexpected character '!'");
          return false;
        }
        continue;
      case '"':
        pos_ = start;
        line_ = sl;
        column_ = sc;
        lex_string(t);
        push_token(t);
        continue;
      default:
        if (std::isdigit(static_cast<unsigned char>(c))) {
          pos_ = start;
          line_ = sl;
          column_ = sc;
          lex_number(t);
          push_token(t);
        } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
          pos_ = start;
          line_ = sl;
          column_ = sc;
          lex_ident_or_keyword(t);
          push_token(t);
        } else {
          SourceLoc loc{file_, sl, sc, start};
          diags.error(loc, std::string("unexpected character '") + c + "'");
          return false;
        }
        continue;
    }
  }

  if (body_mode_) {
    while (indent_stack_.size() > 1) {
      indent_stack_.pop_back();
      Token t;
      t.kind = TokenKind::Dedent;
      t.line = line_;
      t.column = 1;
      t.start = pos_;
      t.end = pos_;
      push_token(t);
    }
    body_mode_ = false;
  }
  Token eof;
  eof.kind = TokenKind::Eof;
  push_token(eof);
  return diags.empty();
}

}  // namespace li
