## Summary

## Release notes (required — [policy](https://github.com/li-langverse/roadmap/blob/main/docs/ecosystem/release-notes.md))

Link: `docs/release-notes/YYYY-MM-DD-<slug>.md` (skill **write-li-release-notes**)

- [ ] **CHANGELOG.md** `## [Unreleased]` updated
- [ ] Dated `docs/release-notes/` file with **Agent continuation** + **Not changed** sections

## Engineering gates (mandatory)

- [ ] **Functionality** — `li-tests` / phase gate; REQ/PH ids
- [ ] **Security** — CVE catalog / `run_security.sh` if attack surface changed
- [ ] **Performance** — benchmark or documented N/A
- [ ] **Learned from** — if new ecosystem feature (2–4 references)
- [ ] **Coverage** — `std/**` 100% if touched; packages ≥80% when `lit` applies
- [ ] **Vision** — master plan updated if cross-repo / pillar change
- [ ] **Roadmap governance** PR opened/merged (human) if pillars or ecosystem policy changed
- [ ] **Agent-kit** — `./scripts/sync-agent-kit.sh` if roadmap `agent-kit/` version bumped

## Test plan

- [ ] `./scripts/ci.sh` (and `ci-security.sh` if applicable)
