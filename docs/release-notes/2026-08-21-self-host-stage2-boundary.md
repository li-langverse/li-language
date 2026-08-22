# Self-hosting boundary: stage-2 frontend verified, Layer 6 remains

The Phase 6 bootstrap now has a dedicated `scripts/check_li_stage2_frontend.sh`
gate. The C++ host compiles `bootstrap/lic/main.li`, and the resulting
`lic-from-li` binary reproduces the C++ lexer and canonical AST dump on its own
source. This is the first stage-2 self-host boundary and runs in `scripts/ci.sh`.

This is intentionally **not** called complete self-hosting. The resulting
binary still exposes the bootstrap front-end and MIR slice, but does not yet
implement `build`. The C++ host still performs:

- complete module/import and package resolution;
- the full MIR lowering surface, including the remaining object/generic gaps;
- MIR-to-LLVM code generation and runtime linking;
- proof/build policy and release artifact generation.

The next milestone is Layer 6: port the validated C++ codegen/build driver into
Li, then make a Li-compiled stage-2 binary compile `bootstrap/lic/main.li`
without invoking the C++ host. Only that stage-3 binary, followed by the same
parity and end-to-end tests, can retire the C++ host from the production path.
The C++ compiler remains the reference host under `.cursor/skills/upgrade-li-from-cpp/SKILL.md`.
