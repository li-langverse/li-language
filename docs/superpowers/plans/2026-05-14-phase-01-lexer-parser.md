# Phase 1: Lexer + Parser (C++)

**Goal:** Tokenize and parse Nim-like Li surface syntax into an AST.

**Architecture:** `compiler/lexer` → `compiler/parser` → `compiler/ast`; diagnostics in `compiler/diagnostics`.

**Tech stack:** C++17

**Exit gate:**
```bash
./scripts/build.sh
LIC=./build/compiler/lic/lic ./li-tests/run_all.sh lexer_parser
```

**CLI:** `lic parse <file.li>`
