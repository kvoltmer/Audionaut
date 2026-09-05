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
// Note for agents: an Autosave.json inside a package is the GUI app's private
// crash-recovery snapshot - never read or edit it; the project state lives in
// Project.json.
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
    description:
      "Renders the project offline to a WAV file (no audio device needed). Pass region to bounce a " +
      "single region instead - always dry, without any clip's gains or fades.",
    inputSchema: {
      project: projectParam,
      output: z.string().describe("Output .wav path"),
      sample_rate: z.number().int().positive().optional().describe("Sample rate in Hz (default 44100)"),
      bit_depth: z.number().int().positive().optional().describe("Bit depth (default 24)"),
      channels: z.number().int().min(1).optional().describe("Output channel count (default 2)"),
      multi_mono: z.boolean().optional().describe("Write one mono file per channel instead"),
      start_seconds: z.number().min(0).optional().describe("Export start position"),
      length_seconds: z.number().positive().optional().describe("Export length (default: whole project)"),
      region: z.string().min(1).optional().describe("Bounce this region instead of the whole project"),
      track: z.number().int().min(0).optional().describe("Track id, to disambiguate same-named regions"),
    },
  },
  async ({ project, output, sample_rate, bit_depth, channels, multi_mono, start_seconds, length_seconds,
           region, track }) =>
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
      ...(region !== undefined ? ["--region", region] : []),
      ...(track !== undefined ? ["--track", String(track)] : []),
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

// Timeline positions are musical by default: bars/beats are 1-based (bar 1 =
// timeline start, 4/4 assumed app-wide), seconds/clocks absolute from 0.
const unitParam = z
  .enum(["bars", "beats", "seconds", "clocks"])
  .optional()
  .describe("Unit for positions (default bars; bars/beats are 1-based, seconds/clocks absolute)");

server.registerTool(
  "split",
  {
    title: "Split clips",
    description:
      "Splits the clip under the given timeline position into two on every track that has one there " +
      "(the GUI's Split command). Fails with nothing_to_split when no clip spans the position.",
    inputSchema: {
      project: projectParam,
      at: z.number().describe("Timeline position to split at, e.g. 23 for bar 23"),
      unit: unitParam,
    },
  },
  async ({ project, at, unit }) =>
    runCli(["split", project, "--at", String(at), ...(unit ? ["--unit", unit] : [])])
);

server.registerTool(
  "create_region",
  {
    title: "Create region",
    description:
      "Creates a named region from a timeline range on every track whose clip fully contains it " +
      "(the GUI's Create Region command). The arrangement itself is unchanged.",
    inputSchema: {
      project: projectParam,
      name: z.string().min(1).describe("Name for the new region"),
      start: z.number().describe("Range start, e.g. 23 for bar 23"),
      end: z.number().describe("Range end (must be after start)"),
      unit: unitParam,
    },
  },
  async ({ project, name, start, end, unit }) =>
    runCli([
      "create-region",
      project,
      "--name",
      name,
      "--start",
      String(start),
      "--end",
      String(end),
      ...(unit ? ["--unit", unit] : []),
    ])
);

server.registerTool(
  "set_region",
  {
    title: "Edit region",
    description:
      "Renames a region and/or retrims its source-relative range (clamped to the source audio). " +
      "Clips have no length of their own, so a retrim affects every clip using the region.",
    inputSchema: {
      project: projectParam,
      region: z.string().min(1).describe("Name of the region to edit"),
      rename: z.string().min(1).optional().describe("New name (must not collide with an existing region)"),
      start: z.number().optional().describe("New source-relative start"),
      end: z.number().optional().describe("New source-relative end (exclusive with length)"),
      length: z.number().positive().optional().describe("New length, keeping the start"),
      unit: unitParam,
      track: z.number().int().min(0).optional().describe("Track id, to disambiguate same-named regions"),
    },
  },
  async ({ project, region, rename, start, end, length, unit, track }) =>
    runCli([
      "set-region",
      project,
      "--region",
      region,
      ...(rename !== undefined ? ["--rename", rename] : []),
      ...(start !== undefined ? ["--start", String(start)] : []),
      ...(end !== undefined ? ["--end", String(end)] : []),
      ...(length !== undefined ? ["--length", String(length)] : []),
      ...(unit ? ["--unit", unit] : []),
      ...(track !== undefined ? ["--track", String(track)] : []),
    ])
);

server.registerTool(
  "remove_clip",
  {
    title: "Remove clips",
    description:
      "Removes the clip at a timeline position (one clip; ambiguity across tracks needs track), or every " +
      "placement of a named region. delete_region also drops the region itself unless other clips use it.",
    inputSchema: {
      project: projectParam,
      at: z.number().optional().describe("Timeline position of the clip (exclusive with region)"),
      region: z.string().min(1).optional().describe("Region name; removes every placement"),
      unit: unitParam,
      track: z.number().int().min(0).optional().describe("Track id"),
      delete_region: z.boolean().optional().describe("Also delete the region from the pool"),
    },
  },
  async ({ project, at, region, unit, track, delete_region }) =>
    runCli([
      "remove-clip",
      project,
      ...(at !== undefined ? ["--at", String(at)] : []),
      ...(region !== undefined ? ["--region", region] : []),
      ...(unit ? ["--unit", unit] : []),
      ...(track !== undefined ? ["--track", String(track)] : []),
      ...(delete_region ? ["--delete-region"] : []),
    ])
);

server.registerTool(
  "move_clip",
  {
    title: "Move clip",
    description:
      "Moves one clip to a new timeline position on its track. Address it by position (at) or region name; " +
      "the address must match exactly one clip.",
    inputSchema: {
      project: projectParam,
      to: z.number().describe("Target timeline position"),
      at: z.number().optional().describe("Current position of the clip (exclusive with region)"),
      region: z.string().min(1).optional().describe("Region name of the clip"),
      unit: unitParam,
      track: z.number().int().min(0).optional().describe("Track id"),
    },
  },
  async ({ project, to, at, region, unit, track }) =>
    runCli([
      "move-clip",
      project,
      "--to",
      String(to),
      ...(at !== undefined ? ["--at", String(at)] : []),
      ...(region !== undefined ? ["--region", region] : []),
      ...(unit ? ["--unit", unit] : []),
      ...(track !== undefined ? ["--track", String(track)] : []),
    ])
);

server.registerTool(
  "place_clip",
  {
    title: "Place clip",
    description:
      "Creates a new clip from an existing named region at a timeline position, on the track that owns " +
      "the region (track only disambiguates same-named regions).",
    inputSchema: {
      project: projectParam,
      region: z.string().min(1).describe("Name of the region to place"),
      at: z.number().describe("Timeline position for the new clip"),
      unit: unitParam,
      track: z.number().int().min(0).optional().describe("Track id, to disambiguate same-named regions"),
    },
  },
  async ({ project, region, at, unit, track }) =>
    runCli([
      "place-clip",
      project,
      "--region",
      region,
      "--at",
      String(at),
      ...(unit ? ["--unit", unit] : []),
      ...(track !== undefined ? ["--track", String(track)] : []),
    ])
);

server.registerTool(
  "cleanup_regions",
  {
    title: "Delete unused regions",
    description:
      "Deletes every region that has no clip on the timeline (the GUI's Delete Unused Regions). " +
      "The audio files stay in the package.",
    inputSchema: { project: projectParam },
  },
  async ({ project }) => runCli(["cleanup-regions", project])
);

server.registerTool(
  "clip_gain",
  {
    title: "Set clip gain",
    description:
      "Sets the addressed clip's gain - linear by default, dB with db=true - on every destination " +
      "channel, or one channel. The address (at or region) must match exactly one clip.",
    inputSchema: {
      project: projectParam,
      gain: z.number().describe("Gain value (linear unless db=true; linear must be >= 0)"),
      db: z.boolean().optional().describe("Interpret gain as decibels"),
      channel: z.number().int().min(0).optional().describe("One destination channel (default: all)"),
      at: z.number().optional().describe("Timeline position of the clip (exclusive with region)"),
      region: z.string().min(1).optional().describe("Region name of the clip"),
      unit: unitParam,
      track: z.number().int().min(0).optional().describe("Track id"),
    },
  },
  async ({ project, gain, db, channel, at, region, unit, track }) =>
    runCli([
      "clip-gain",
      project,
      "--gain",
      String(gain),
      ...(db ? ["--db"] : []),
      ...(channel !== undefined ? ["--channel", String(channel)] : []),
      ...(at !== undefined ? ["--at", String(at)] : []),
      ...(region !== undefined ? ["--region", region] : []),
      ...(unit ? ["--unit", unit] : []),
      ...(track !== undefined ? ["--track", String(track)] : []),
    ])
);

server.registerTool(
  "clip_speed",
  {
    title: "Set clip speed",
    description:
      "Sets a clip's playback speed (ratio 0.25-4, 2.0 = double speed, half as long). The mode picks how: " +
      "repitch (default, varispeed - pitch and length change together) or stretch (pitch-preserving " +
      "time-stretch). Mode can also be changed on its own.",
    inputSchema: {
      project: projectParam,
      at: z.string().optional().describe("Timeline position of the clip (in `unit`)"),
      region: z.string().optional().describe("Region name of the clip"),
      track: z.number().int().min(0).optional().describe("Track id to narrow the match"),
      ratio: z.number().positive().optional().describe("Speed ratio (0.25-4)"),
      semitones: z.number().optional().describe("Pitch shift in semitones (ratio = 2^(n/12))"),
      length: z.string().optional().describe("Fit the clip to this timeline duration (in `unit`)"),
      mode: z.enum(["repitch", "stretch"]).optional()
        .describe("How the speed is realised: varispeed or pitch-preserving"),
      unit: unitParam,
    },
  },
  async ({ project, at, region, track, ratio, semitones, length, mode, unit }) =>
    runCli([
      "clip-speed",
      project,
      ...(at !== undefined ? ["--at", at] : []),
      ...(region !== undefined ? ["--region", region] : []),
      ...(track !== undefined ? ["--track", String(track)] : []),
      ...(ratio !== undefined ? ["--ratio", String(ratio)] : []),
      ...(semitones !== undefined ? ["--semitones", String(semitones)] : []),
      ...(mode !== undefined ? ["--mode", mode] : []),
      ...(length !== undefined ? ["--length", length] : []),
      ...(unit !== undefined ? ["--unit", unit] : []),
    ])
);

server.registerTool(
  "separate_stems",
  {
    title: "Separate stems",
    description:
      "Splits a clip into Drums/Bass/Other/Vocals tracks with the Demucs (htdemucs) source separator, " +
      "aligned with the source clip. Slow: minutes for a full song, CPU only. Needs the model weights, " +
      "which the Audionaut app downloads once (Settings > Separation); fails with model_missing until then, " +
      "and with demucs_unavailable in builds without Demucs.",
    inputSchema: {
      project: projectParam,
      track: z.number().int().min(0).optional().describe("Track id (default 0; imported audio lands on a new track)"),
      clip: z.number().int().optional().describe("Playlist item id (default: the track's first clip)"),
      threads: z.number().int().positive().optional().describe("Parallel segments (default: physical cores)"),
      mute_source: z.boolean().optional().describe("Mute the source track's channels (default true)"),
    },
  },
  async ({ project, track, clip, threads, mute_source }) =>
    runCli([
      "separate",
      project,
      ...(track !== undefined ? ["--track", String(track)] : []),
      ...(clip !== undefined ? ["--clip", String(clip)] : []),
      ...(threads !== undefined ? ["--threads", String(threads)] : []),
      ...(mute_source === false ? ["--no-mute-source"] : []),
    ])
);

server.registerTool(
  "clip_fades",
  {
    title: "Set clip fades",
    description:
      "Sets the addressed clip's fade lengths, ramp offsets (0 clears; offsets may be negative to reach " +
      "outside the clip) and curve exponents (0.1-4, 0.5 = equal power). Values are clamped against each " +
      "other within the clip. The address must match exactly one clip.",
    inputSchema: {
      project: projectParam,
      fade_in: z.number().min(0).optional().describe("Fade-in length"),
      fade_out: z.number().min(0).optional().describe("Fade-out length"),
      fade_in_start: z.number().optional().describe("Fade-in ramp start offset from the clip start"),
      fade_out_end: z.number().optional().describe("Fade-out ramp end offset from the clip end"),
      fade_in_curve: z.number().optional().describe("Fade-in curve exponent (0.1-4, 0.5 = equal power)"),
      fade_out_curve: z.number().optional().describe("Fade-out curve exponent (0.1-4, 0.5 = equal power)"),
      at: z.number().optional().describe("Timeline position of the clip (exclusive with region)"),
      region: z.string().min(1).optional().describe("Region name of the clip"),
      unit: unitParam,
      track: z.number().int().min(0).optional().describe("Track id"),
    },
  },
  async ({ project, fade_in, fade_out, fade_in_start, fade_out_end, fade_in_curve, fade_out_curve,
           at, region, unit, track }) =>
    runCli([
      "clip-fades",
      project,
      ...(fade_in !== undefined ? ["--fade-in", String(fade_in)] : []),
      ...(fade_out !== undefined ? ["--fade-out", String(fade_out)] : []),
      ...(fade_in_start !== undefined ? ["--fade-in-start", String(fade_in_start)] : []),
      ...(fade_out_end !== undefined ? ["--fade-out-end", String(fade_out_end)] : []),
      ...(fade_in_curve !== undefined ? ["--fade-in-curve", String(fade_in_curve)] : []),
      ...(fade_out_curve !== undefined ? ["--fade-out-curve", String(fade_out_curve)] : []),
      ...(at !== undefined ? ["--at", String(at)] : []),
      ...(region !== undefined ? ["--region", region] : []),
      ...(unit ? ["--unit", unit] : []),
      ...(track !== undefined ? ["--track", String(track)] : []),
    ])
);

server.registerTool(
  "remove_track",
  {
    title: "Remove track",
    description:
      "Removes a whole track from the project, including its channels, clips and regions. Track ids " +
      "are positions in the track list, so the ids of the tracks below shift up by one. " +
      "The audio files stay in the package.",
    inputSchema: {
      project: projectParam,
      track: z.number().int().min(0).describe("Track id"),
    },
  },
  async ({ project, track }) =>
    runCli(["remove-track", project, "--track", String(track)])
);

server.registerTool(
  "remove_channel",
  {
    title: "Remove channel",
    description:
      "Removes one channel (0-based) from a track, like deleting a channel strip in the GUI. " +
      "Channel mappings and per-channel clip gains shift down; the audio files stay in the package.",
    inputSchema: {
      project: projectParam,
      track: z.number().int().min(0).describe("Track id"),
      channel: z.number().int().min(0).describe("Channel index within the track (0-based)"),
    },
  },
  async ({ project, track, channel }) =>
    runCli(["remove-channel", project, "--track", String(track), "--channel", String(channel)])
);

const transport = new StdioServerTransport();
await server.connect(transport);
console.error(`audionaut-mcp ready (cli: ${cliPath})`);
