# Org backup (li-langverse experiment)

<!-- DOC-ecosystem-org-backup -->

**Audience:** Maintainers running the agent-open **`li-langverse`** org experiment.

| | |
|--|--|
| **Live org** | [`li-langverse`](https://github.com/li-langverse) (public repos) |
| **Backup org** | [`li-langverse-backup`](https://github.com/li-langverse-backup) (private mirrors, auto-created) |
| **Local mirrors** | `~/Documents/li-langverse-backup/mirrors/li-langverse/<name>.git` |
| **Report** | `~/Documents/li-langverse-backup/LAST_SUCCESS.txt` |

## Credentials (agents must not use)

| File | Purpose |
|------|---------|
| `coding-projects/.env.github` | Agent PAT |
| `coding-projects/.env.github.backup` | `BACKUP_GH_TOKEN`, `BACKUP_OWNER=li-langverse-backup` |

Copy [`scripts/env.github.backup.example`](../../scripts/env.github.backup.example), `chmod 600`.

**Agents must not:** run `backup-*`, `export-li-langverse-metadata`, `recovery-drill-*`, or `with-github-backup-env`; read `.env.github.backup`. Cursor hook [`guard-backup.sh`](../../.cursor/hooks/guard-backup.sh) blocks these unless `LI_HOOK_ALLOW=1`.

### After the experiment (narrow agent PAT)

| Scope | Target |
|-------|--------|
| Remove | Org admin on `li-langverse` |
| Keep | Per-repo **Contents: Write** on repos agents edit (`lic`, `lip`, `lit`, …) |
| Enable | Branch protection on `main` (required reviews or block direct push) |
| Rotate | All Actions secrets if admin PAT was exposed |

## Run backup

```bash
cd /path/to/li
./scripts/backup-li-langverse-org.sh
./scripts/export-li-langverse-metadata.sh
```

- Discovers all repos via `gh api orgs/li-langverse/repos`
- Bare-clones or `remote update --prune` locally
- Auto-creates missing **`li-langverse-backup/<name>`** (private)
- Pushes **branches + tags** only (not `refs/pull/*` — GitHub rejects PR refs on backup repos)

## Daily cron (optional)

```bash
0 3 * * * /path/to/li/scripts/backup-li-langverse-org.sh && /path/to/li/scripts/export-li-langverse-metadata.sh
```

## Recovery drill (verify backups)

```bash
./scripts/recovery-drill-li-langverse-backup.sh
```

Creates `li-langverse-backup/lic-recovery-drill` from the local `lic` mirror, verifies `main` SHA matches, deletes the drill repo.

**Gaps (by design):** PR refs, issue/PR bodies, secret values, registry packages — not in git mirrors. Use `metadata/*/repo-*-rulesets.json` to re-apply branch protection after restore.

## Recovery (live org damaged)

1. Revoke agent PAT in `.env.github`; rotate `LI_DOWNSTREAM_DISPATCH_TOKEN`, `LIC_CHECKOUT_TOKEN`, and repo secrets.
2. Org → Audit log; compare `LAST_SUCCESS.txt` to live `git ls-remote`.
3. For each repo: `gh repo create li-langverse/<name> --public`, then from `mirrors/li-langverse/<name>.git`:
   ```bash
   git push --force https://github.com/li-langverse/<name>.git \
     'refs/heads/*:refs/heads/*' 'refs/tags/*:refs/tags/*'
   ```
   Or push from **`li-langverse-backup/<name>`** if local disk is lost.
4. Re-apply rulesets from `metadata/*/repo-*-rulesets.json`.
5. Re-wire remotes and downstream workflows per [upstream-notifications.md](upstream-notifications.md).

## Scripts

| Script | Role |
|--------|------|
| [`backup-li-langverse-org.sh`](../../scripts/backup-li-langverse-org.sh) | Mirror + auto-create backup org repos |
| [`export-li-langverse-metadata.sh`](../../scripts/export-li-langverse-metadata.sh) | JSON under `metadata/<timestamp>/` |
| [`recovery-drill-li-langverse-backup.sh`](../../scripts/recovery-drill-li-langverse-backup.sh) | Throwaway restore test |
| [`with-github-backup-env.sh`](../../scripts/with-github-backup-env.sh) | Loads `.env.github.backup` only |

## Related

- [org-push.md](org-push.md) — push lic/lip/lit to live org
- [governance.md](governance.md) — org policy (canonical in roadmap repo)
- [agent-coordination.md](agent-coordination.md) — multi-agent rules
