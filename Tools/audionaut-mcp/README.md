# audionaut-mcp

An [MCP](https://modelcontextprotocol.io) server that lets AI agents (Claude
Code, Claude Desktop, and any other MCP client) work with Audionaut projects.
It is a thin wrapper: every tool shells out to `audionaut-cli` with `--json`
and relays the result — no engine logic lives here.

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

A typical agent flow: `create_project` → `import_audio` → `analyze` →
`auto_edit`/`assemble` → `export_audio`. CLI errors come back as tool errors
carrying the CLI's own `code: message` (e.g. `essentia_unavailable: ...` in
builds without Essentia), so agents can react.

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
