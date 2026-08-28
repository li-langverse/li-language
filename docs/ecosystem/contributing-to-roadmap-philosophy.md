# Contributing: roadmap Philosophy.md

The sibling [`li-langverse/roadmap`](https://github.com/li-langverse/roadmap) repo has no `Philosophy.md` yet. Paste the block below into **`docs/Philosophy.md`** (new file) or append under a **Principles** heading if the file already exists.

Human merge required for roadmap governance paths.

---

## Composability

Li treats every substantial feature — webservers, benchmark runners, package tools — as a **small, composable API**, not a monolithic binary. Other programs and agents must be able to `import` a library, call `serve` / `stop` / `ready` (or domain-equivalent verbs), and tear down services without copy-paste. Package `src/lib.li` is canonical; `src/main.li` is a thin demo only. Composable surfaces still carry proof contracts under strict-by-default.

**Implementation detail (lic):** [composable-by-default.md](https://github.com/li-langverse/lic/blob/main/docs/ecosystem/composable-by-default.md)

---

After merge in roadmap, optionally link from `docs/ecosystem/vision-and-roadmap.md` in that repo.
