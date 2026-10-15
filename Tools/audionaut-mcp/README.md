# audionaut-mcp

An [MCP](https://modelcontextprotocol.io) server that lets AI agents (Claude
Code, Claude Desktop, and any other MCP client) edit
[Audionaut](https://audionaut.app) projects: cut, move, fade, analyse,
auto-edit and export. It is a thin wrapper: every tool runs one Audionaut
command with `--json` and relays the result — no engine logic lives here.

**Requires the Audionaut app** ([Mac App Store](https://apps.apple.com/app/id6743627933) /
[GitHub Releases](https://github.com/kvoltmer/Audionaut/releases)). On its own
this package does nothing.

## Quick start

You need [Audionaut](https://audionaut.app/download) — 1.6.3 or later is
recommended; 1.6.2 works on macOS and Linux — and [Node.js](https://nodejs.org)
18 or later.

Claude Code:

```
claude mcp add audionaut -- npx -y audionaut-mcp
```

Claude Desktop (`claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "audionaut": {
      "command": "npx",
      "args": ["-y", "audionaut-mcp"]
    }
  }
}
```

On Windows, if Claude Desktop cannot start `npx`, use
`"command": "cmd", "args": ["/c", "npx", "-y", "audionaut-mcp"]`.

Then ask for something like *"split the clip in ~/Music/Audionaut/Demo
Project.audium every 16 bars"*. Keep the project open in Audionaut to watch
the edits arrive; each one is a single undo step.

**On macOS, keep projects in your Music folder.** Audionaut is sandboxed
there (App Store and website download alike) and can only reach `~/Music`:
projects, audio to import and export targets all have to be inside it, for
example in `~/Music/Audionaut`. The server refuses other paths up front with
`sandbox_denied` and a hint. Windows and Linux have no such limit.

To see which Audionaut the server found and whether it answers:

```
npx -y audionaut-mcp --check
```

The server looks for, in order: `AUDIONAUT_CLI`; a developer build of
`audionaut-cli` (see the end of this page); `audionaut-cli` on `PATH`; the
installed app — `/Applications` or `~/Applications` (or wherever Spotlight
finds it) on macOS, `Program Files\Audionaut` on Windows, `audionaut` from
the `.deb` or an `Audionaut*.AppImage` in `~/Applications` or `~/.local/bin`
on Linux. Anywhere else, point `AUDIONAUT_CLI` at the binary, e.g. your
AppImage.

`separate_stems` needs the Demucs model, which you download once in the app
(Settings ▸ Separation). The model weights are licensed for research use.

A stem split takes minutes for a full song, and exporting or analysing a long
project can too. When your client sends a `progressToken` with the call,
`separate_stems`, `export_audio` and `analyze` report progress (Audionaut
1.6.5 and later). A call is only stopped after ten minutes without any sign of
life, not ten minutes in total.

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

`Autosave.json` and `Host.json` inside a package belong to the app — never
read or edit them.

## Tools

| Tool | Wraps | Purpose |
|---|---|---|
| `get_project_info` | `info` | Tempo, tracks, clips, files (or the raw persistence JSON) |
| `create_project` | `create` | New empty `.audium` package |
| `import_audio` | `import` | Add audio files (creates a new track) |
| `export_audio` | `export` | Offline render to WAV, FLAC, AIFF, Ogg Vorbis or MP3 |
| `analyze` | `analyze` | Essentia analysis, cached next to the project |
| `auto_edit` | `auto-edit` | Segment a clip using cached analysis |
| `assemble` | `assemble` | Build an arrangement from regions |
| `split` | `split` | Split clips at a timeline position (bars/beats/seconds/clocks) |
| `create_region` | `create-region` | Create a named region from a timeline range |
| `set_region` | `set-region` | Rename and/or retrim a region (affects all its clips) |
| `remove_clip` | `remove-clip` | Remove clip(s) from the timeline |
| `move_clip` | `move-clip` | Move one clip to a new position and/or another track |
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

## Environment variables

| Variable | Effect |
|---|---|
| `AUDIONAUT_CLI` | Binary that runs the commands: an Audionaut app or `audionaut-cli` |
| `AUDIONAUT_SKIP_PATH_CHECK` | `1` skips the `~/Music` pre-check for the sandboxed macOS app |
| `AUDIONAUT_DISABLE_ANALYTICS` | `1` never sends usage analytics, whatever the app's consent setting |
| `AUDIONAUT_FEATURE_REQUEST_URL` | Issue relay for `request_feature`/`report_bug`; empty skips it |
| `AUDIONAUT_DISABLE_FEATURE_REQUESTS` | `1` never files issues; replies carry a prefilled link instead |

## Developing: building the CLI from source

From a clone of the repo, the server prefers a CMake build of the
standalone `audionaut-cli`, which is not sandboxed:

1. Build the CLI (from the repo root):

   ```
   cmake -B build -S Audionaut/Catch2Tests
   cmake --build build -j8 --target AudionautCli
   ```

2. Install the server's dependencies:

   ```
   cd Tools/audionaut-mcp && npm install
   ```

3. Register the checkout instead of the npm package. Claude Code:

   ```
   claude mcp add audionaut -- node /path/to/Audionaut/Tools/audionaut-mcp/index.js
   ```

   Claude Desktop: the JSON above with `"command": "node"` and
   `"args": ["/path/to/Audionaut/Tools/audionaut-mcp/index.js"]`.

`npm test` drives the server through a real MCP client against whichever
binary it finds (`AUDIONAUT_CLI` picks one). With the sandboxed macOS app it
works in a scratch folder under `~/Music`; `AUDIONAUT_TEST_DIR` overrides
the location.

Paths in tool arguments are best given absolute — relative paths resolve
against the server process's working directory, which depends on the MCP
client.
