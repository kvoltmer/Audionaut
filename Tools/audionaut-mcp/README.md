# audionaut-mcp

An [MCP](https://modelcontextprotocol.io) server that lets AI agents (Claude
Code, Claude Desktop, and any other MCP client) work with Audionaut projects.
It is a thin wrapper: every tool shells out to `audionaut-cli` with `--json`
and relays the result — no engine logic lives here.

## Projects that are open in Audionaut

The tools do not need the user to save first, and they will not write over a
document the user has open. When Audionaut is holding the project, the CLI
hands the command to the app: it acts on the document as the user currently
sees it, unsaved changes included, and the result lands in their undo history
as one step instead of touching `Project.json`. Saving stays theirs to do.

With no app holding the project, the tools read and write the project file as
they always have. If an app is holding one but cannot be reached, the command
fails (`host_unavailable`) rather than falling back to the file, which would
be missing whatever is unsaved.

Two caveats. A command running inside the app runs inside its sandbox, so an
export outside the user's Music folder is refused with `sandbox_denied`. And
`Autosave.json` and `Host.json` inside a package belong to the app — never
read or edit them.

## Tools

| Tool | Wraps | Purpose |
|---|---|---|
| `get_project_info` | `info` | Tempo, tracks, clips, files (or the raw persistence JSON) |
| `create_project` | `create` | New empty `.audium` package |
| `import_audio` | `import` | Add audio files (creates a new track) |
| `export_audio` | `export` | Offline render to WAV |
| `analyze` | `analyze` | Essentia analysis, cached next to the project |
| `auto_edit` | `auto-edit` | Segment a clip using cached analysis |
| `assemble` | `assemble` | Build an arrangement from regions |
| `split` | `split` | Split clips at a timeline position (bars/beats/seconds/clocks) |
| `create_region` | `create-region` | Create a named region from a timeline range |
| `set_region` | `set-region` | Rename and/or retrim a region (affects all its clips) |
| `remove_clip` | `remove-clip` | Remove clip(s) from the timeline |
| `move_clip` | `move-clip` | Move one clip to a new position |
| `place_clip` | `place-clip` | Place an existing region on the timeline |
| `cleanup_regions` | `cleanup-regions` | Delete every region no clip uses |
| `clip_gain` | `clip-gain` | Set a clip's gain (linear or dB, all channels or one) |
| `clip_fades` | `clip-fades` | Set a clip's fade lengths, offsets and curves |
| `clip_speed` | `clip-speed` | Set a clip's speed and mode (varispeed or pitch-preserving stretch), or lock it to the project tempo |
| `remove_track` | `remove-track` | Remove a whole track (channels, clips and regions) |
| `remove_channel` | `remove-channel` | Remove one channel from a track |
| `request_feature` | — | Send a feature request to the maintainer (see below) |
| `report_bug` | — | Send a bug report to the maintainer (see below) |
| `separate_stems` | `separate` | Split a clip into Drums/Bass/Other/Vocals tracks (Demucs; needs the downloaded model) |

A typical agent flow: `create_project` → `import_audio` → `analyze` →
`auto_edit`/`assemble` → `export_audio`. CLI errors come back as tool errors
carrying the CLI's own `code: message` (e.g. `essentia_unavailable: ...` in
builds without Essentia), so agents can react.

## Feature requests and bug reports from agents

Agents run into the edges of what the tools expose, and into their bugs,
long before a person would file an issue, so two tools give them a direct
channel. `request_feature` takes a title, a description and (ideally) what
the agent was trying to do and which tool fell short. `report_bug` takes
the same plus steps to reproduce, the expected behaviour and the project
shape; its description tells agents to quote the exact tool call and error
text and never to attach audio. The server's instructions point agents at
the right one whenever a task needs something the tools cannot do or a tool
misbehaves, and tell them to say so to the user. A `reporter` name or e-mail
is included only when the user offers one. Both become GitHub issues -
`enhancement` + `agent-request` for features, `bug` + `agent-report` for
bugs; the reply carries the issue URL and the
[feature-request discussion](https://github.com/kvoltmer/Audionaut/discussions/63).

Transports, tried in order:

1. **Relay** — the default: a deployment of
   [`Tools/feature-request-relay`](../feature-request-relay/README.md), a
   Cloudflare Worker that files the issue with its own GitHub token. This is
   the path for end users, who have no GitHub credentials on the agent side.
   `AUDIONAUT_FEATURE_REQUEST_URL` points at another deployment; an empty
   value skips the relay.
2. **GitHub CLI** — when the relay is skipped and `gh` is installed and
   logged in, the issue is filed with `gh issue create` under the user's own
   account (developer machines).
3. **Nothing** — otherwise the reply says `sent: false` and carries a
   prefilled new-issue URL for the user to open themselves.

`AUDIONAUT_DISABLE_FEATURE_REQUESTS=1` skips the first two for both tools
(test harnesses, air-gapped setups); the smoke test points the relay URL at
a local stand-in.

## Setup

1. Build the CLI (from the repo root):

   ```
   cmake -B build -S Audionaut/Catch2Tests
   cmake --build build -j8 --target AudionautCli
   ```

2. Install the server's dependencies:

   ```
   cd Tools/audionaut-mcp && npm install
   ```

3. Register with your MCP client. Claude Code:

   ```
   claude mcp add audionaut -- node /path/to/Audionaut/Tools/audionaut-mcp/index.js
   ```

   Claude Desktop (`claude_desktop_config.json`):

   ```json
   {
     "mcpServers": {
       "audionaut": {
         "command": "node",
         "args": ["/path/to/Audionaut/Tools/audionaut-mcp/index.js"]
       }
     }
   }
   ```

The server finds `audionaut-cli` in the repo's `build/` directory
automatically; point `AUDIONAUT_CLI` at the binary to override (e.g. an
installed copy), or put `audionaut-cli` on `PATH`.

Paths in tool arguments are best given absolute — relative paths resolve
against the server process's working directory, which depends on the MCP
client.
