//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

// Smoke test: drives the MCP server through the SDK's stdio client and runs
// the full agent flow against a scratch directory. Requires a built
// audionaut-cli (see README). Run with `npm test`.

import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = join(here, "..", "..");
const testFiles = join(repoRoot, "Audionaut", "Catch2Tests", "TestFiles");

let failures = 0;
function check(name, condition, detail = "") {
  console.log(`${condition ? "ok  " : "FAIL"} ${name}${condition ? "" : `  ${detail}`}`);
  if (!condition) failures++;
}

const transport = new StdioClientTransport({ command: "node", args: [join(here, "index.js")] });
const client = new Client({ name: "audionaut-mcp-test", version: "0.0.1" });
await client.connect(transport);

const workDir = mkdtempSync(join(tmpdir(), "audionaut-mcp-test-"));

try {
  const { tools } = await client.listTools();
  const names = tools.map((tool) => tool.name).sort();
  check(
    "all seven tools listed",
    JSON.stringify(names) ===
      JSON.stringify(["analyze", "assemble", "auto_edit", "create_project", "export_audio", "get_project_info", "import_audio"]),
    names.join(",")
  );

  const info = await client.callTool({
    name: "get_project_info",
    arguments: { project: join(testFiles, "Sessions", "simple-sine.audium") },
  });
  check("info on checked-in session", !info.isError && info.content[0].text.includes("tempoBpm"));

  const missing = await client.callTool({
    name: "get_project_info",
    arguments: { project: join(workDir, "missing.audium") },
  });
  check("missing project is a tool error", missing.isError === true, missing.content?.[0]?.text);

  const project = join(workDir, "flow.audium");
  const created = await client.callTool({ name: "create_project", arguments: { project, channels: 1 } });
  check("create_project", !created.isError, created.content?.[0]?.text);

  const imported = await client.callTool({
    name: "import_audio",
    arguments: { project, files: [join(testFiles, "120-funk-1-sec.wav")] },
  });
  check("import_audio", !imported.isError, imported.content?.[0]?.text);

  const exported = await client.callTool({
    name: "export_audio",
    arguments: { project, output: join(workDir, "mix.wav"), channels: 1 },
  });
  check("export_audio", !exported.isError && exported.content[0].text.includes("mix.wav"), exported.content?.[0]?.text);

  // Analysis needs Essentia; accept either success or a clean unavailable error.
  const analyzed = await client.callTool({ name: "analyze", arguments: { target: project } });
  const analyzeClean = !analyzed.isError || analyzed.content[0].text.startsWith("essentia_unavailable");
  check("analyze succeeds or reports essentia_unavailable", analyzeClean, analyzed.content?.[0]?.text);

  if (!analyzed.isError) {
    // imported audio lands on a new track (id 1)
    const edited = await client.callTool({
      name: "auto_edit",
      arguments: { project, track: 1, segments: 4 },
    });
    check("auto_edit", !edited.isError, edited.content?.[0]?.text);
  }
} finally {
  rmSync(workDir, { recursive: true, force: true });
  await client.close();
}

process.exit(failures === 0 ? 0 : 1);
