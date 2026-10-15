//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

// Smoke test: drives the MCP server through the SDK's stdio client and runs
// the full agent flow against a scratch directory. Needs something that runs
// the CLI verbs - a built audionaut-cli or an installed Audionaut app (see
// README; AUDIONAUT_CLI picks one explicitly). Run with `npm test`.
//
// The sandboxed macOS app can only reach ~/Music, so the scratch directory
// goes there for it (AUDIONAUT_TEST_DIR overrides) and the fixtures are
// copied in rather than read from the repo.

import { chmodSync, cpSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { createServer } from "node:http";
import { homedir, tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";

import { outsideMusicFolder, parseEnvelope, parseProgressLine, resolveCli, userPath } from "./locate.js";

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = join(here, "..", "..");
const repoTestFiles = join(repoRoot, "Audionaut", "Catch2Tests", "TestFiles");

let failures = 0;
function check(name, condition, detail = "") {
  console.log(`${condition ? "ok  " : "FAIL"} ${name}${condition ? "" : `  ${detail}`}`);
  if (!condition) failures++;
}

// --- locate.js: discovery and path handling, no binary needed -------------
{
  const scratch = mkdtempSync(join(tmpdir(), "audionaut-locate-"));
  const noRepo = join(scratch, "no-repo");

  const missing = await resolveCli({ env: { AUDIONAUT_CLI: join(scratch, "nope") }, repoRoot: noRepo });
  check("AUDIONAUT_CLI pointing nowhere is reported", missing.path === null && /no executable/.test(missing.error));

  const none = await resolveCli({ env: { PATH: "" }, platform: "linux", home: scratch, repoRoot: noRepo });
  check("nothing installed resolves to no binary", none.path === null && none.searched.length > 0);

  mkdirSync(join(scratch, "Applications"));
  const image = join(scratch, "Applications", "Audionaut-1.6.3-x86_64.AppImage");
  writeFileSync(image, "#!/bin/sh\n");
  chmodSync(image, 0o755);
  const appImage = await resolveCli({ env: { PATH: "" }, platform: "linux", home: scratch, repoRoot: noRepo });
  check("AppImage in ~/Applications is found", appImage.path === image && !appImage.sandboxed);

  const mac = await resolveCli({
    env: { AUDIONAUT_CLI: image }, platform: "darwin", repoRoot: noRepo, spotlight: async () => [],
  });
  check("only a macOS .app bundle counts as sandboxed", mac.path === image && !mac.sandboxed);

  check("~ expands to the user's home", userPath("~/Music/a.audium", { home: "/h" }) === join("/h", "Music", "a.audium"));
  check("relative paths resolve against cwd", userPath("a.audium", { cwd: "/w" }) === resolve("/w", "a.audium"));

  mkdirSync(join(scratch, "Music"));
  check("paths in ~/Music pass, even ones not created yet",
        outsideMusicFolder([join(scratch, "Music", "new", "mix.wav")], { home: scratch }) === null);
  check("paths outside ~/Music are named",
        outsideMusicFolder([join(scratch, "Music", "a"), join(scratch, "b.wav")], { home: scratch }) === join(scratch, "b.wav"));

  check("a plain envelope parses", parseEnvelope('{"ok":true,"result":1}')?.result === 1);
  check("a log line ahead of the envelope is skipped (Linux 1.6.2)",
        parseEnvelope('settings: /home/u/.config/x.settings\n{\n  "ok": true\n}\n')?.ok === true);
  check("output without an envelope is rejected", parseEnvelope("segfault") === null && parseEnvelope("") === null);

  const tick = parseProgressLine('{"message":"Separating stems","progress":0.35}');
  check("a --progress-json line parses", tick?.fraction === 0.35 && tick?.message === "Separating stems");
  check("other stderr lines are not progress",
        parseProgressLine("JUCE v9.0.2") === null && parseProgressLine("{") === null &&
        parseProgressLine('{"ok":true}') === null);

  rmSync(scratch, { recursive: true, force: true });
}

// --- progress relay, against a stand-in CLI --------------------------------
// The CLI is `node` itself and the verb runs as a script: `node separate ...`
// loads ./separate from the server's working directory. That works on every
// CI platform, where a shell script would not. Its ticks span longer than the
// shortened idle timeout, so finishing proves each tick restarts the clock.
{
  const scratch = mkdtempSync(join(tmpdir(), "audionaut-progress-"));
  const standIn = `const args = process.argv.slice(2);
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
(async () => {
  process.stderr.write("JUCE v9.0.2\\n");
  if (args[0].includes("hang")) await sleep(3000);
  for (const [message, progress] of [["Rendering clip", 0], ["Separating stems", 0.25], ["Separating stems", 0.25],
                                     ["Separating stems", 0.5], ["Separating stems", 0.75], ["Writing stems", 1]]) {
    process.stderr.write(JSON.stringify({ message, progress }) + "\\n");
    await sleep(300);
  }
  process.stdout.write(JSON.stringify({ ok: true, result: { args } }));
})();
`;
  for (const verb of ["separate", "export", "analyze"]) writeFileSync(join(scratch, verb), standIn);

  const transport = new StdioClientTransport({
    command: "node",
    args: [join(here, "index.js")],
    cwd: scratch,
    env: { ...process.env, AUDIONAUT_CLI: process.execPath, AUDIONAUT_SKIP_PATH_CHECK: "1",
           AUDIONAUT_CLI_IDLE_TIMEOUT_MS: "1000", AUDIONAUT_DISABLE_ANALYTICS: "1" },
  });
  const client = new Client({ name: "audionaut-mcp-progress-test", version: "0.0.1" });
  await client.connect(transport);

  try {
    const seen = [];
    const separated = await client.callTool(
      { name: "separate_stems", arguments: { project: join(scratch, "song.audium") } },
      undefined,
      { onprogress: (progress) => seen.push(progress) }
    );
    check("separate_stems outlives the idle timeout while it reports progress", !separated.isError,
          separated.content?.[0]?.text);
    check("the CLI is asked for --progress-json",
          separated.content?.[0]?.text.includes("--progress-json"), separated.content?.[0]?.text);
    check("progress arrives as percent of 100, banner skipped, repeat dropped",
          JSON.stringify(seen.map((p) => p.progress)) === "[0,25,50,75,100]" &&
            seen.every((p) => p.total === 100) && seen[0].message === "Rendering clip",
          JSON.stringify(seen));

    for (const [name, args] of [["export_audio", { project: join(scratch, "song.audium"), output: join(scratch, "mix.wav") }],
                                ["analyze", { target: join(scratch, "song.audium") }]]) {
      const ticks = [];
      const done = await client.callTool({ name, arguments: args }, undefined,
                                         { onprogress: (progress) => ticks.push(progress.progress) });
      check(`${name} relays progress`, !done.isError && JSON.stringify(ticks) === "[0,25,50,75,100]",
            `${JSON.stringify(ticks)} ${done.content?.[0]?.text}`);
    }

    const hung = await client.callTool({ name: "separate_stems", arguments: { project: join(scratch, "hang.audium") } });
    check("a CLI silent past the idle timeout is stopped with a tool error",
          hung.isError === true && /no sign of life/.test(hung.content?.[0]?.text), hung.content?.[0]?.text);
  } finally {
    await client.close();
    rmSync(scratch, { recursive: true, force: true });
  }
}

const cli = await resolveCli();
if (!cli.path) {
  console.log("FAIL no Audionaut binary found - build audionaut-cli, install the app, or set AUDIONAUT_CLI");
  process.exit(1);
}
console.log(`     using ${cli.path} (${cli.source})`);

const workDir = process.env.AUDIONAUT_TEST_DIR
  ? mkdtempSync(join(process.env.AUDIONAUT_TEST_DIR, "audionaut-mcp-test-"))
  : cli.sandboxed
    ? (mkdirSync(join(homedir(), "Music"), { recursive: true }),
       mkdtempSync(join(homedir(), "Music", "audionaut-mcp-test-")))
    : mkdtempSync(join(tmpdir(), "audionaut-mcp-test-"));

const testFiles = join(workDir, "fixtures");
for (const fixture of [join("Sessions", "simple-sine.audium"), "120-funk-1-sec.wav", "sine-0dB.wav"])
  cpSync(join(repoTestFiles, fixture), join(testFiles, fixture), { recursive: true });

// Stand-in for the issue relay: records what request_feature / report_bug
// post and rejects titles containing "reject" so the error path is covered.
const featureRequests = [];
const featureEndpoint = createServer((request, response) => {
  let raw = "";
  request.on("data", (chunk) => (raw += chunk));
  request.on("end", () => {
    const payload = JSON.parse(raw);
    featureRequests.push(payload);
    const reject = payload.title.includes("reject");
    response.writeHead(reject ? 400 : 200, { "Content-Type": "application/json" });
    response.end(JSON.stringify(reject ? { success: false, message: "rejected" }
                                       : { success: true, url: "https://github.com/kvoltmer/Audionaut/issues/0" }));
  });
});
await new Promise((resolve) => featureEndpoint.listen(0, "127.0.0.1", resolve));
const featureEndpointUrl = `http://127.0.0.1:${featureEndpoint.address().port}/submit`;

const transport = new StdioClientTransport({
  command: "node",
  args: [join(here, "index.js")],
  // the smoke suite must never send usage analytics, whatever the local
  // consent preference says - and feature requests go to the stand-in above
  env: { ...process.env, AUDIONAUT_DISABLE_ANALYTICS: "1", AUDIONAUT_FEATURE_REQUEST_URL: featureEndpointUrl },
});
const client = new Client({ name: "audionaut-mcp-test", version: "0.0.1" });
await client.connect(transport);

try {
  const { tools } = await client.listTools();
  const names = tools.map((tool) => tool.name).sort();
  check(
    "all twenty-two tools listed",
    JSON.stringify(names) ===
      JSON.stringify(["analyze", "assemble", "auto_edit", "cleanup_regions", "clip_fades", "clip_gain",
                      "clip_speed", "create_project", "create_region", "export_audio", "get_project_info",
                      "import_audio", "move_clip", "place_clip", "remove_channel", "remove_clip",
                      "remove_track", "report_bug", "request_feature", "separate_stems", "set_region",
                      "split"]),
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

  if (cli.sandboxed) {
    const outside = await client.callTool({
      name: "get_project_info",
      arguments: { project: join(tmpdir(), "outside.audium") },
    });
    check("project outside ~/Music is refused up front", outside.isError === true &&
          outside.content?.[0]?.text.startsWith("sandbox_denied"), outside.content?.[0]?.text);
  }

  const project = join(workDir, "flow.audium");
  const created = await client.callTool({ name: "create_project", arguments: { project, channels: 1 } });
  check("create_project", !created.isError, created.content?.[0]?.text);

  const imported = await client.callTool({
    name: "import_audio",
    arguments: { project, files: [join(testFiles, "120-funk-1-sec.wav")] },
  });
  check("import_audio", !imported.isError, imported.content?.[0]?.text);

  // Out-of-range times are refused before the CLI runs (the CLI itself
  // rejects them too, in its --json envelope, for callers that bypass MCP).
  const negativePosition = await client.callTool({
    name: "import_audio",
    arguments: { project, files: [join(testFiles, "sine-0dB.wav")], position_seconds: -2 },
  });
  check("negative position_seconds is a tool error", negativePosition.isError === true,
        negativePosition.content?.[0]?.text);
  const zeroLength = await client.callTool({
    name: "export_audio",
    arguments: { project, output: join(workDir, "never.wav"), length_seconds: 0 },
  });
  check("zero length_seconds is a tool error", zeroLength.isError === true, zeroLength.content?.[0]?.text);

  const exported = await client.callTool({
    name: "export_audio",
    arguments: { project, output: join(workDir, "mix.wav"), channels: 1 },
  });
  check("export_audio", !exported.isError && exported.content[0].text.includes("mix.wav"), exported.content?.[0]?.text);

  const flac = await client.callTool({
    name: "export_audio",
    arguments: { project, output: join(workDir, "mix.flac"), channels: 1 },
  });
  check("export_audio to FLAC", !flac.isError && flac.content[0].text.includes('"format": "flac"'),
        flac.content?.[0]?.text);

  const ogg = await client.callTool({
    name: "export_audio",
    arguments: { project, output: join(workDir, "mix.ogg"), channels: 1, bitrate_kbps: 112 },
  });
  check("export_audio to Ogg Vorbis with a bit rate",
        !ogg.isError && ogg.content[0].text.includes('"format": "ogg"') &&
          ogg.content[0].text.includes('"bitrateKbps": 112'),
        ogg.content?.[0]?.text);

  const mp3 = await client.callTool({
    name: "export_audio",
    arguments: { project, output: join(workDir, "mix.mp3"), channels: 1 },
  });
  check("export_audio to MP3 at the default bit rate",
        !mp3.isError && mp3.content[0].text.includes('"format": "mp3"') &&
          mp3.content[0].text.includes('"bitrateKbps": 192'),
        mp3.content?.[0]?.text);

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

  // Structure edits last: they remove the strips the flow above worked on.
  const channelGone = await client.callTool({
    name: "remove_channel",
    arguments: { project, track: 0, channel: 0 },
  });
  check(
    "remove_channel",
    !channelGone.isError && channelGone.content[0].text.includes("remainingChannels"),
    channelGone.content?.[0]?.text
  );

  const channelMiss = await client.callTool({
    name: "remove_channel",
    arguments: { project, track: 0, channel: 9 },
  });
  check("remove_channel out of range is a tool error", channelMiss.isError === true, channelMiss.content?.[0]?.text);

  const trackGone = await client.callTool({
    name: "remove_track",
    arguments: { project, track: 1 },
  });
  check(
    "remove_track",
    !trackGone.isError && trackGone.content[0].text.includes("removedTrack"),
    trackGone.content?.[0]?.text
  );

  const trackMiss = await client.callTool({
    name: "remove_track",
    arguments: { project, track: 5 },
  });
  check("remove_track on missing id is a tool error", trackMiss.isError === true, trackMiss.content?.[0]?.text);

  // Feature requests post to the stand-in endpoint, never to the real one.
  const requested = await client.callTool({
    name: "request_feature",
    arguments: {
      title: "Duplicate clip",
      description: "There is no tool to duplicate a clip in place.",
      context: "Wanted to repeat a 4-bar loop; used place_clip four times instead.",
      reporter: "smoke@example.com",
    },
  });
  const sentPayload = featureRequests.at(-1);
  check(
    "request_feature is sent via the relay",
    !requested.isError && requested.content[0].text.includes('"via": "relay"') &&
      requested.content[0].text.includes("issues/0"),
    requested.content?.[0]?.text
  );
  check(
    "request_feature payload carries title, description, context and reporter",
    sentPayload?.title === "Duplicate clip" &&
      sentPayload?.kind === "feature" &&
      sentPayload?.body.includes("no tool to duplicate") &&
      sentPayload?.body.includes("place_clip four times") &&
      sentPayload?.reporter === "smoke@example.com" &&
      sentPayload?.client.startsWith("audionaut-mcp"),
    JSON.stringify(sentPayload)
  );

  const bug = await client.callTool({
    name: "report_bug",
    arguments: {
      title: "split leaves an empty region",
      description: "split at a clip boundary reported ok but created a zero-length region.",
      steps: "1. import_audio 120-funk-1-sec.wav\n2. split at 1.0 seconds",
      expected: "A usage error, or no region at all.",
      context: "1 track, 1 clip, 44.1 kHz",
    },
  });
  const bugPayload = featureRequests.at(-1);
  check(
    "report_bug is sent via the relay",
    !bug.isError && bug.content[0].text.includes('"via": "relay"'),
    bug.content?.[0]?.text
  );
  check(
    "report_bug payload is kind bug with steps, expected and context",
    bugPayload?.kind === "bug" &&
      bugPayload?.body.includes("**Steps to reproduce**") &&
      bugPayload?.body.includes("split at 1.0 seconds") &&
      bugPayload?.body.includes("**Expected**") &&
      bugPayload?.body.includes("44.1 kHz"),
    JSON.stringify(bugPayload)
  );

  const rejected = await client.callTool({
    name: "request_feature",
    arguments: { title: "please reject this", description: "endpoint error path" },
  });
  check(
    "request_feature endpoint failure is a tool error with the issue fallback",
    rejected.isError === true && rejected.content[0].text.includes("rejected") &&
      rejected.content[0].text.includes("issues/new?"),
    rejected.content?.[0]?.text
  );
} finally {
  rmSync(workDir, { recursive: true, force: true });
  await client.close();
  featureEndpoint.close();
}

process.exit(failures === 0 ? 0 : 1);
