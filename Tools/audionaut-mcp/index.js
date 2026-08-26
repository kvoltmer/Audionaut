#!/usr/bin/env node
//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

// MCP server wrapping audionaut-cli. Every tool shells out to the CLI with
// --json and relays the {ok, result|error} envelope - no engine logic lives
// here. See README.md for registration instructions.

import { execFile } from "node:child_process";
import { access, constants } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { promisify } from "node:util";

import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";

const execFileAsync = promisify(execFile);
const accessAsync = promisify(access);

// ---------------------------------------------------------------------------
// CLI location: $AUDIONAUT_CLI, else the repo's CMake build output (this
// package lives at <repo>/Tools/audionaut-mcp), else audionaut-cli on PATH.
// Single-config generators (Makefile/Ninja) emit AudionautCli_artefacts/
// directly; multi-config ones (Xcode, Visual Studio) add a configuration
// subdirectory, and Windows adds .exe.
// ---------------------------------------------------------------------------
async function resolveCliPath() {
  if (process.env.AUDIONAUT_CLI) return process.env.AUDIONAUT_CLI;

  const repoRoot = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const artefacts = join(repoRoot, "build", "AudionautCli_artefacts");
  const binary = process.platform === "win32" ? "AudionautCli.exe" : "AudionautCli";

  for (const candidate of [
    join(artefacts, binary),
    join(artefacts, "Release", binary),
    join(artefacts, "Debug", binary),
  ]) {
    try {
      await accessAsync(candidate, constants.X_OK);
      return candidate;
    } catch {
      // try the next layout
    }
  }

  return "audionaut-cli"; // hope it's on PATH; spawn errors are surfaced per call
}

const cliPath = await resolveCliPath();

// Runs one CLI command and converts its --json envelope into an MCP tool
// result. CLI failures come back as isError results (the agent can read the
// code/message and adjust), never as thrown protocol errors.
async function runCli(args) {
  let stdout;
  try {
    ({ stdout } = await execFileAsync(cliPath, [...args, "--json", "--quiet"], {
      timeout: 10 * 60 * 1000, // export/analyze of long projects can be slow
      maxBuffer: 32 * 1024 * 1024, // --raw project dumps can be large
    }));
  } catch (error) {
    // Non-zero exit still writes the envelope to stdout; prefer it over the
    // raw error so the agent sees the CLI's own code/message.
    stdout = error.stdout;
    if (!stdout) {
      const message =
        error.code === "ENOENT"
          ? `audionaut-cli not found at "${cliPath}". Build it (cmake -B build -S Audionaut/Catch2Tests && ` +
            `cmake --build build --target AudionautCli) or set AUDIONAUT_CLI to the binary.`
          : `audionaut-cli failed: ${error.message}`;
      return { content: [{ type: "text", text: message }], isError: true };
    }
  }

  let envelope;
  try {
    envelope = JSON.parse(stdout);
  } catch {
    return {
      content: [{ type: "text", text: `audionaut-cli returned unparseable output: ${String(stdout).slice(0, 2000)}` }],
      isError: true,
    };
  }

  if (envelope.ok)
    return { content: [{ type: "text", text: JSON.stringify(envelope.result, null, 2) }] };

  return {
    content: [{ type: "text", text: `${envelope.error?.code}: ${envelope.error?.message}` }],
    isError: true,
  };
}

// Shared parameter fragments
const projectParam = z
  .string()
  .describe("Path to the .audium project package (absolute paths recommended)");

const server = new McpServer({ name: "audionaut", version: "0.1.0" });

server.registerTool(
  "get_project_info",
  {
    title: "Get project info",
    description:
      "Summarizes an Audionaut project: tempo, tracks, clips with positions/durations and their audio files. " +
      "Set raw=true for the full persistence JSON instead of the summary.",
    inputSchema: {
      project: projectParam,
      raw: z.boolean().optional().describe("Dump the full persistence JSON instead of the summary"),
    },
  },
  async ({ project, raw }) => runCli(["info", project, ...(raw ? ["--raw"] : [])])
);

server.registerTool(
  "create_project",
  {
    title: "Create project",
    description:
      "Creates a new empty Audionaut project (.audium package) with one track. Fails if the path already exists.",
    inputSchema: {
      project: projectParam.describe("Target path for the new .audium package"),
      channels: z.number().int().min(1).optional().describe("Channels on the initial track (default 2)"),
    },
  },
  async ({ project, channels }) =>
    runCli(["create", project, ...(channels ? ["--channels", String(channels)] : [])])
);

server.registerTool(
  "import_audio",
  {
    title: "Import audio",
    description:
      "Imports one or more audio files into the project at the given position and saves it. " +
      "Note: importing creates a new track holding the files.",
    inputSchema: {
      project: projectParam,
      files: z.array(z.string()).min(1).describe("Audio file paths to import"),
      position_seconds: z.number().min(0).optional().describe("Timeline position for the files (default 0)"),
    },
  },
  async ({ project, files, position_seconds }) =>
    runCli([
      "import",
      project,
      ...files,
      ...(position_seconds !== undefined ? ["--position", String(position_seconds)] : []),
    ])
);

server.registerTool(
  "export_audio",
  {
    title: "Export audio",
    description: "Renders the project offline to a WAV file (no audio device needed).",
    inputSchema: {
      project: projectParam,
      output: z.string().describe("Output .wav path"),
      sample_rate: z.number().int().positive().optional().describe("Sample rate in Hz (default 44100)"),
      bit_depth: z.number().int().positive().optional().describe("Bit depth (default 24)"),
      channels: z.number().int().min(1).optional().describe("Output channel count (default 2)"),
      multi_mono: z.boolean().optional().describe("Write one mono file per channel instead"),
      start_seconds: z.number().min(0).optional().describe("Export start position"),
      length_seconds: z.number().positive().optional().describe("Export length (default: whole project)"),
    },
  },
  async ({ project, output, sample_rate, bit_depth, channels, multi_mono, start_seconds, length_seconds }) =>
    runCli([
      "export",
      project,
      "-o",
      output,
      ...(sample_rate ? ["--sample-rate", String(sample_rate)] : []),
      ...(bit_depth ? ["--bit-depth", String(bit_depth)] : []),
      ...(channels ? ["--channels", String(channels)] : []),
      ...(multi_mono ? ["--multi-mono"] : []),
      ...(start_seconds !== undefined ? ["--start", String(start_seconds)] : []),
      ...(length_seconds !== undefined ? ["--length", String(length_seconds)] : []),
    ])
);

server.registerTool(
  "analyze",
  {
    title: "Analyze audio",
    description:
      "Runs Essentia audio analysis (segment boundaries, beats, BPM) on the project's audio files - or one " +
      "standalone audio file - and caches the results next to the project for auto_edit/assemble to use. " +
      "Fails with essentia_unavailable in builds without Essentia.",
    inputSchema: {
      target: z.string().describe("A .audium project package or a single audio file"),
      types: z
        .string()
        .optional()
        .describe("Comma-separated analysis types, e.g. \"sbic,beat_degara\" (default: the merge set)"),
    },
  },
  async ({ target, types }) => runCli(["analyze", target, ...(types ? ["--types", types] : [])])
);

server.registerTool(
  "auto_edit",
  {
    title: "Auto-edit clip",
    description:
      "Segments a clip on a track using cached analysis results (run analyze first). " +
      "Splits the clip into musically-aligned regions.",
    inputSchema: {
      project: projectParam,
      track: z.number().int().min(0).optional().describe("Track id (default 0; imported audio lands on a new track)"),
      clip: z.number().int().optional().describe("Playlist item id (default: the track's clip)"),
      measures: z.number().positive().optional().describe("Segment length in measures"),
      segments: z.number().int().positive().optional().describe("Number of segments"),
      duration_seconds: z.number().positive().optional().describe("Target duration"),
      crossfades: z.boolean().optional().describe("Apply crossfades at joints (default true)"),
    },
  },
  async ({ project, track, clip, measures, segments, duration_seconds, crossfades }) =>
    runCli([
      "auto-edit",
      project,
      ...(track !== undefined ? ["--track", String(track)] : []),
      ...(clip !== undefined ? ["--clip", String(clip)] : []),
      ...(measures !== undefined ? ["--measures", String(measures)] : []),
      ...(segments !== undefined ? ["--segments", String(segments)] : []),
      ...(duration_seconds !== undefined ? ["--duration", String(duration_seconds)] : []),
      ...(crossfades === false ? ["--no-crossfades"] : []),
    ])
);

server.registerTool(
  "assemble",
  {
    title: "Assemble arrangement",
    description:
      "Builds an arrangement of the given duration from the project's regions, sequentially or at random " +
      "(requires regions - e.g. from auto_edit).",
    inputSchema: {
      project: projectParam,
      track: z.number().int().min(0).optional().describe("Track id (default 0)"),
      duration_seconds: z.number().positive().optional().describe("Target duration (default 60)"),
      mode: z.enum(["random", "sequential"]).optional().describe("Selection mode (default sequential)"),
      seed: z.number().int().optional().describe("Random seed for reproducible random mode"),
    },
  },
  async ({ project, track, duration_seconds, mode, seed }) =>
    runCli([
      "assemble",
      project,
      ...(track !== undefined ? ["--track", String(track)] : []),
      ...(duration_seconds !== undefined ? ["--duration", String(duration_seconds)] : []),
      ...(mode ? ["--mode", mode] : []),
      ...(seed !== undefined ? ["--seed", String(seed)] : []),
    ])
);

const transport = new StdioServerTransport();
await server.connect(transport);
console.error(`audionaut-mcp ready (cli: ${cliPath})`);
