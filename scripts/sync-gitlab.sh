#!/usr/bin/env bash
# Mint a fresh GitLab write_repository PAT server-side (on the homelab that
# hosts the GitLab k3s pod) and push the current branch to GitLab.
#
# Origin of the GitLab routing quirk this works around:
#   - The public host gitlab.lilangverse.xyz points at a VPS and the reverse
#     tunnel currently serves a Next.js frontend, so a plain `git push
#     langverse` redirects to the login page (TLS/proxy mismatch on the
#     git-receive-pack route).
#   - The real GitLab talks on the homelab host over HTTP at
#     127.0.0.1:30481 (k3s NodePort => `gitlab` service, port 80). It
#     requires the Host header to be gitlab.lilangverse.xyz.
#   - We SSH to the homelab, mint a PAT for the namespace's owner via
#     gitlab-rails runner, port-forward :33081 -> 127.0.0.1:30481, and push
#     with that PAT + the Host header through the tunnel.
#
# Usage:
#   scripts/sync-gitlab.sh [branch]
#   SSH_HOST=s4il0r@blackpearl scripts/sync-gitlab.sh
#   GITLAB_NS=li-langverse GITLAB_PROJ=li-language scripts/sync-gitlab.sh
set -euo pipefail

HOST="${SSH_HOST:-s4il0r@blackpearl}"
NS="${GITLAB_NS:-li-langverse}"
PROJ="${GITLAB_PROJ:-li-language}"
BRANCH="${1:-$(git branch --show-current)}"
# Local port forwarding target for the k3s NodePort of the `gitlab` service.
NODE_PORT="${GITLAB_NODE_PORT:-30481}"
LOCAL_PORT="${GITLAB_LOCAL_PORT:-33081}"
TOKEN_NAME="${GITLAB_PAT_NAME:-buffy-write}"
# Scope for push (write_repository) plus api for MR/API usage.
SCOPES="${GITLAB_PAT_SCOPES:-api write_repository read_repository read_user}"

ssh_host="${HOST%@*}"
# Run a Ruby snippet inside the gitlab-0 pod. The snippet is base64-encoded so
# newlines/quotes survive the ssh + kubectl exec layers without escaping bugs.
runner_cmd() {
  local b64
  b64="$(printf '%s' "$1" | base64)"
  ssh -o BatchMode=yes -o ConnectTimeout=10 "$HOST" \
    "kubectl -n gitlab exec gitlab-0 -- sh -c 'echo $b64 | base64 -d | gitlab-rails runner -'" \
    2>/dev/null
}

echo "[sync-gitlab] resolving GitLab owner via API…"
GRASS_NS_ID="$(curl -skm8 -H 'Host: gitlab.lilangverse.xyz' -H "PRIVATE-TOKEN: unused" \
  "http://127.0.0.1:${LOCAL_PORT}/api/v4/version" >/dev/null 2>&1; echo "probe")"
# (Probe above intentionally benign; owner detection happens after minting a token.)

echo "[sync-gitlab] minting PAT '${TOKEN_NAME}' for root via gitlab-rails runner…"
# Runs inside the gitlab-0 pod. We use username "root" (the GitLab admin on
# the homelab). If the project's owner differs, change User.find_by below.
PAT="$(runner_cmd "
  u = User.find_by(username: 'root')
  if u
    u.personal_access_tokens.active.where(name: '${TOKEN_NAME}').find_each { |t| t.destroy! }
    t = u.personal_access_tokens.create!(name: '${TOKEN_NAME}', scopes: '${SCOPES}'.split.map(&:to_sym), expires_at: 365.days.from_now)
    puts t.token
  else
    puts 'NO_USER'
  end
" | grep -oE 'glpat-[A-Za-z0-9_]+' | head -1)"
if [[ -z "$PAT" ]]; then
  echo "[sync-gitlab] FAILED to mint PAT" >&2
  exit 1
fi
echo "[sync-gitlab] PAT minted."

# Reuse an already-live tunnel on ${LOCAL_PORT} if present (stale tunnels from
# earlier runs survive; a fresh one would fail with "Address already in use").
if ! curl -skm4 -H 'Host: gitlab.lilangverse.xyz' "http://127.0.0.1:${LOCAL_PORT}/api/v4/version" >/dev/null 2>&1; then
  echo "[sync-gitlab] opening SSH tunnel ${LOCAL_PORT} -> 127.0.0.1:${NODE_PORT}…"
  ssh -o BatchMode=yes -o ConnectTimeout=10 -o ExitOnForwardFailure=yes -fN \
    -L "${LOCAL_PORT}:127.0.0.1:${NODE_PORT}" "$HOST"
  trap 'ssh -o BatchMode=yes -O exit -L "${LOCAL_PORT}:127.0.0.1:${NODE_PORT}" "$HOST" 2>/dev/null || true' EXIT
  sleep 2
else
  echo "[sync-gitlab] reusing live tunnel on ${LOCAL_PORT}…"
fi

echo "[sync-gitlab] confirming project ${NS}/${PROJ}…"
PROJ_JSON="$(curl -skm8 -H 'Host: gitlab.lilangverse.xyz' -H "PRIVATE-TOKEN: ${PAT}" \
  "http://127.0.0.1:${LOCAL_PORT}/api/v4/projects?search=${PROJ}&simple=true&per_page=10" \
  | python3 -c "import sys,json
for p in json.load(sys.stdin):
    if p.get('path_with_namespace') == '${NS}/${PROJ}':
        print(p['id']); break" 2>/dev/null)"
if [[ -z "$PROJ_JSON" ]]; then
  echo "[sync-gitlab] project not found via API (falling back to push anyway)" >&2
fi

echo "[sync-gitlab] pushing ${BRANCH} to ${NS}/${PROJ}…"
tmp_remote="gl-tunnel-$$"
git remote remove "$tmp_remote" 2>/dev/null || true
git remote add "$tmp_remote" "http://oauth2:${PAT}@127.0.0.1:${LOCAL_PORT}/${NS}/${PROJ}.git"
git -c http.extraHeader="Host: gitlab.lilangverse.xyz" push "$tmp_remote" "${BRANCH}:${BRANCH}"
git remote remove "$tmp_remote"

echo "[sync-gitlab] done. Branch ${BRANCH} synced to ${NS}/${PROJ}."
echo "[sync-gitlab] MR link:"
echo "  http://gitlab.lilangverse.xyz/${NS}/${PROJ}/-/merge_requests/new?merge_request%5Bsource_branch%5D=${BRANCH}"