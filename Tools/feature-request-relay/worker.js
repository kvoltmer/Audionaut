//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

// Cloudflare Worker relaying feature requests from audionaut-mcp into GitHub
// issues. It holds the only credential (GITHUB_TOKEN, a fine-grained PAT with
// Issues read/write on the repo) so agents on end-user machines need none.
// Request: POST JSON {title, body, reporter?, client?}
// Reply:   {success: true, url, number} or {success: false, message}
// GET answers a health check {ok, repo, hasToken} without touching GitHub.

const MAX_TITLE = 120;
const MAX_BODY = 8000;

function json(payload, status = 200) {
  return new Response(JSON.stringify(payload), {
    status,
    headers: { "Content-Type": "application/json" },
  });
}

export default {
  async fetch(request, env) {
    // GET is a health check: says whether a token is configured and whether
    // GitHub accepts it (the status of an authenticated /user call), never
    // what the token is.
    if (request.method === "GET") {
      const token = String(env.GITHUB_TOKEN ?? "").trim();
      let githubStatus = null;
      if (token) {
        const probe = await fetch("https://api.github.com/user", {
          headers: {
            Authorization: `Bearer ${token}`,
            Accept: "application/vnd.github+json",
            "User-Agent": "audionaut-feature-request-relay",
          },
        });
        githubStatus = probe.status;
      }
      return json({ ok: true, repo: env.GITHUB_REPO, hasToken: Boolean(token), githubStatus });
    }
    if (request.method !== "POST") return json({ success: false, message: "POST only" }, 405);

    let payload;
    try {
      payload = await request.json();
    } catch {
      return json({ success: false, message: "invalid JSON" }, 400);
    }

    const title = String(payload.title ?? "").trim().slice(0, MAX_TITLE);
    const body = String(payload.body ?? "").trim().slice(0, MAX_BODY);
    if (!title || !body) return json({ success: false, message: "title and body are required" }, 400);

    const client = String(payload.client ?? "unknown client").slice(0, 200);
    const token = String(env.GITHUB_TOKEN ?? "").trim(); // a pasted secret may carry a newline
    if (!token) return json({ success: false, message: "relay has no GITHUB_TOKEN" }, 500);
    const labels = (env.LABELS ?? "enhancement,agent-request").split(",").map((label) => label.trim());

    const response = await fetch(`https://api.github.com/repos/${env.GITHUB_REPO}/issues`, {
      method: "POST",
      headers: {
        Authorization: `Bearer ${token}`,
        Accept: "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "audionaut-feature-request-relay",
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ title, body: `${body}\n\n_Relayed from ${client}_`, labels }),
    });

    if (!response.ok) {
      let detail = "";
      try {
        detail = (await response.json()).message ?? "";
      } catch {
        // no JSON body; the status alone will do
      }
      return json({ success: false, message: `GitHub answered ${response.status}${detail ? `: ${detail}` : ""}` }, 502);
    }

    const issue = await response.json();
    return json({ success: true, url: issue.html_url, number: issue.number });
  },
};
