#pragma once

#include "li/ast.hpp"

#include <string>
#include <string_view>

namespace li {

/// Canonical int-encoded AST dump used as the self-host parity contract.
///
/// `lic ast <file>` prints one node per line as `code field...`. The li
/// bootstrap parser (bootstrap/lic/main.li `ast <file>` subcommand) emits the
/// byte-identical stream from its own parse; scripts/check_li_ast_parity.sh
/// diffs the two. Node codes (int-encoded like bootstrap/prover/main.li):
///
///   1 IMPORT         <module> <alias>
///   2 ERROR          <name> <template>
///   3 THEOREM        <name> <is_axiom> <is_lemma>   children: params, proposition
///   4 PROC           <name> <vis> <is_extern> <is_async>
///   5 PROC_TYPE_PARAM <name> <bound>
///   6 PARAM          <name>                        child: type
///   7 CONTRACT       <kind> <given> <samples>      child: expr
///   8 DECORATOR      <name>                        children: args
///   9 DECORATOR_ARG  <name>                        child: value expr
///   10 RAISES        <name>
///   20 TYPE_NAMED    <name> <is_var>
///   21 TYPE_ARRAY    <is_var> <size>               child: elem
///   22 TYPE_REFINEMENT <is_var> <var>              children: base, pred
///   23 TYPE_APP      <name> <is_var> <variadic>    children: args
///   24 TYPE_CALLABLE <is_var>                      children: args, ret
///   25 TYPE_GENERIC_PARAM <name> <is_var>
///   26 TYPE_NAMED_TUPLE <is_var> <variadic>        children: fields
///   27 TYPE_FIELD    <name> <optional> <vis>       child: type
///   28 TYPE_SIMD     <is_var> <size>               children: type args
///   40 STMT_RETURN / 41 STMT_IF / 42 STMT_WHILE / 45 STMT_BREAK /
///   46 STMT_CONTINUE / 47 STMT_EXPR / 50 STMT_ASSIGN  (children follow)
///   43 STMT_FOR      <iter> <start> <end>          children: contracts, body
///   44 STMT_PARALLEL_FOR <iter> <start> <end>      children: contracts, body
///   48 STMT_VAR_DECL <name>                        children: type, init?
///   49 STMT_BORROW   <name> <is_mut>               child: init
///   51 BLOCK_OPEN / 52 BLOCK_CLOSE (around every statement block)
///   60 EXPR_INT / 61 EXPR_FLOAT / 62 EXPR_BINARY / 63 EXPR_STRING /
///   64 EXPR_IDENT    <lexeme>
///   65 EXPR_BINOP    <op-token-kind>               children: lhs, rhs
///   66 EXPR_CALL     <callee-name>                 children: args
///   67 EXPR_UNARY_NOT / 71 EXPR_AWAIT              child: operand
///   68 EXPR_INDEX                                  children: base, index
///   69 EXPR_FIELD    <field>                       child: base
///   70 EXPR_METHOD_CALL <method>                   children: base, args
///   80 TYPE_ALIAS    <name> <alias-kind> <base>    children: type params,
///                                                  fields / variants / methods
///                                                  / definition
///   81 TYPE_VARIANT  <name>
///   82 TYPE_VARIADIC (tuple `...` marker)           trailing marker node
///   83 TYPE_SIMD_SIZE <size>                        trailing marker node
///
/// Emission order mirrors the recursive-descent parse on the li side, so
/// postfix nodes (Index/Field/MethodCall) and BinOp print *after* their base /
/// lhs operand subtree — both implementations stream the same sequence.
std::string dump_module_ast(const Module& m, std::string_view source);

}  // namespace li
