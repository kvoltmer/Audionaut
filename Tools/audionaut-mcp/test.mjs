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

const transport = new StdioClientTransport({
  command: "node",
  args: [join(here, "index.js")],
  // the smoke suite must never send usage analytics, whatever the local
  // consent preference says
  env: { ...process.env, AUDIONAUT_DISABLE_ANALYTICS: "1" },
});
const client = new Client({ name: "audionaut-mcp-test", version: "0.0.1" });
await client.connect(transport);

const workDir = mkdtempSync(join(tmpdir(), "audionaut-mcp-test-"));

try {
  const { tools } = await client.listTools();
  const names = tools.map((tool) => tool.name).sort();
  check(
    "all sixteen tools listed",
    JSON.stringify(names) ===
      JSON.stringify(["analyze", "assemble", "auto_edit", "cleanup_regions", "clip_fades", "clip_gain",
                      "create_project", "create_region", "export_audio", "get_project_info",
                      "import_audio", "move_clip", "place_clip", "remove_clip", "set_region", "split"]),
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

    const assembled = await client.callTool({
      name: "assemble",
      arguments: { project, track: 1, duration_seconds: 4, mode: "sequential", seed: 42 },
    });
    check(
      "assemble",
      !assembled.isError && assembled.content[0].text.includes("sequential"),
      assembled.content?.[0]?.text
    );
  }

  const gained = await client.callTool({
    name: "clip_gain",
    arguments: { project, at: 0.05, unit: "seconds", gain: -6, db: true },
  });
  check("clip_gain", !gained.isError && gained.content[0].text.includes("gains"), gained.content?.[0]?.text);

  const faded = await client.callTool({
    name: "clip_fades",
    arguments: { project, at: 0.05, unit: "seconds", fade_in: 0.05, fade_out: 0.05 },
  });
  check(
    "clip_fades",
    !faded.isError && faded.content[0].text.includes("fadeInSeconds"),
    faded.content?.[0]?.text
  );

  // Last: these mutate the arrangement, so they run after the auto_edit flow.
  // Sub-second positions keep the range inside the first clip in both layouts
  // (the plain 1 s import, or 0.25 s assembled segments when Essentia ran).
  const region = await client.callTool({
    name: "create_region",
    arguments: { project, name: "smoke-region", start: 0.05, end: 0.2, unit: "seconds" },
  });
  check(
    "create_region",
    !region.isError && region.content[0].text.includes("smoke-region"),
    region.content?.[0]?.text
  );

  const splitMiss = await client.callTool({
    name: "split",
    arguments: { project, at: 1000 },
  });
  check("split outside clips is a tool error", splitMiss.isError === true, splitMiss.content?.[0]?.text);

  const split = await client.callTool({
    name: "split",
    arguments: { project, at: 0.1, unit: "seconds" },
  });
  check("split", !split.isError && split.content[0].text.includes("createdRegions"), split.content?.[0]?.text);

  const trimmed = await client.callTool({
    name: "set_region",
    arguments: { project, region: "smoke-region", length: 0.1, unit: "seconds", rename: "smoke-lead" },
  });
  check(
    "set_region",
    !trimmed.isError && trimmed.content[0].text.includes("smoke-lead"),
    trimmed.content?.[0]?.text
  );

  const placed = await client.callTool({
    name: "place_clip",
    arguments: { project, region: "smoke-lead", at: 30, unit: "seconds" },
  });
  check("place_clip", !placed.isError, placed.content?.[0]?.text);

  const moved = await client.callTool({
    name: "move_clip",
    arguments: { project, region: "smoke-lead", to: 40, unit: "seconds" },
  });
  check("move_clip", !moved.isError && moved.content[0].text.includes("40"), moved.content?.[0]?.text);

  const removed = await client.callTool({
    name: "remove_clip",
    arguments: { project, at: 40, unit: "seconds" },
  });
  check(
    "remove_clip",
    !removed.isError && removed.content[0].text.includes("removedClips"),
    removed.content?.[0]?.text
  );

  const removeMiss = await client.callTool({
    name: "remove_clip",
    arguments: { project, at: 40, unit: "seconds" },
  });
  check("remove_clip on empty spot is a tool error", removeMiss.isError === true, removeMiss.content?.[0]?.text);

  // smoke-lead lost its only clip above, so cleanup must sweep it
  const cleaned = await client.callTool({ name: "cleanup_regions", arguments: { project } });
  check(
    "cleanup_regions",
    !cleaned.isError && cleaned.content[0].text.includes("smoke-lead"),
    cleaned.content?.[0]?.text
  );
} finally {
  rmSync(workDir, { recursive: true, force: true });
  await client.close();
}

process.exit(failures === 0 ? 0 : 1);
