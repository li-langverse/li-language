# li-net — agent scope

**Claim:** `httpd-p0-net` — see [active-agent-claims.md](../../docs/ecosystem/active-agent-claims.md).

**Role:** Trusted TCP `extern` surface (`raises Net`). Implementations live in platform code + `lic` `docs/semantics/trusted.lean`.

**RFC:** [2026-05-16-li-trusted-net-rfc.md](../../docs/superpowers/specs/2026-05-16-li-trusted-net-rfc.md)

**Do not** add `unsafe`, raw syscalls in `.li`, or expand API without RFC + `trusted.lean` update.

**Mirror repo:** `li-langverse/li-net` — sync via `../../scripts/push-official-package-repo.sh li-net`.
