# feature-request-relay

A Cloudflare Worker that turns feature requests sent by
[`audionaut-mcp`](../audionaut-mcp/README.md)'s `request_feature` tool into
GitHub issues. Agents on end-user machines have no GitHub credentials, so the
worker holds the one token and does the filing.

Contract: `POST` JSON `{title, body, reporter?, client?}` → `{success: true,
url, number}` or `{success: false, message}`. Title and body are required and
capped (120 / 8000 characters); issues get the labels in `LABELS`.

## Deploy

1. Create a fine-grained personal access token limited to this repository
   with **Issues: read and write** (a dedicated bot account keeps the filings
   apart from your own).
2. From this directory:

   ```
   npx wrangler login
   npx wrangler secret put GITHUB_TOKEN
   npx wrangler deploy
   ```

3. The printed `https://audionaut-feature-requests.<account>.workers.dev`
   URL is the MCP server's default in `Tools/audionaut-mcp/index.js`; update
   it there if the subdomain changes, or point `AUDIONAUT_FEATURE_REQUEST_URL`
   at another deployment.

A `GET` on the worker is a health check: `{ok, repo, hasToken, githubStatus}`,
where `githubStatus` is GitHub's answer to an authenticated `/user` call
(200 = the token works, 401 = bad credentials). The token is never revealed.
Fine-grained tokens are shown only once, at creation or regeneration - copy
from that green box and store with `pbpaste | npx wrangler secret put
GITHUB_TOKEN` so nothing lands in the shell history.

Deployed 2026-09-13 at
`https://audionaut-feature-requests.feature-request-relay.workers.dev`.

The worker does no rate limiting of its own beyond the size caps; GitHub's
abuse limits on the token apply. Rotate the token if the workers.dev URL
starts receiving junk.
