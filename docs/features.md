# Audionaut features

Audionaut is a free, open-source (GPLv3) multitrack audio editor for macOS,
Windows and Linux. It records, cuts, arranges and exports multitrack
sessions, and it is lighter than a DAW. AI agents such as Claude can edit
sessions over MCP. This page lists what it does. The
[user manual](manual/README.md) has the details.

## Editing

- **Multitrack, multichannel.** A track holds any number of channels that
  play in lockstep, such as an 8-channel live recording or the stems of a
  mix. Clips move all of a track's channels together.
  ([Concepts](manual/05-concepts.md))
- **Region-based, non-destructive.** Named regions are slices of your source
  audio, and clips place them on the timeline. One region can appear many
  times without copying audio. Each track has its own playlist of clips.
- **Split** at the playhead or around a range (Cmd+E), **duplicate**,
  **move**, **delete**, and drag regions from the Regions list onto the
  timeline. ([Editing](manual/06-editing.md))
- **Clip gain** per channel, and **fade-ins and fade-outs** with bendable
  curves (equal-power by default). Fades can reach past a clip's edge into
  the neighbouring material, which is how two clips crossfade.
- **Clip speed** from ×0.25 to ×4, either as varispeed (re-pitch) or as
  pitch-preserving **time-stretch** (Rubber Band R3). **Tempo lock** makes a
  clip follow the project tempo.
- **Snap to grid** on a bar and beat grid, zoom, follow transport, and
  **loop playback** of a selection.
- **Undo for everything**, including recording takes, stem separation and
  agent edits. **Revert to Saved** returns to the file on disk.

## Analysis and Auto Edit

Powered by [Essentia](https://essentia.upf.edu).
([Analysis and Auto Edit](manual/08-analysis-and-auto-edit.md))

- **Analysis:** segment boundaries (SBic), onsets and beat tracking (two
  algorithms). It can run automatically on new audio, results are cached in
  the project, and they can be shown as overlays on a track.
- **Auto Edit, Create Segments:** cuts a clip at analysed, musically
  aligned boundaries, with a live preview (fewer or more segments) and
  optional crossfades at every joint.
- **Auto Edit, Assemble:** builds a new arrangement of a chosen length from
  a track's regions, sequentially or shuffled.

## Stem separation

Splits a clip into **Drums, Bass, Other and Vocals** tracks with Demucs
(htdemucs via demucs.cpp), running locally. Nothing is uploaded. The model
(about 80 MB) is downloaded on first use. Its weights are licensed for
research use only. ([Stem separation](manual/13-stem-separation.md))

## Recording

Records onto armed channels with per-channel input selection and input
monitoring. Each take becomes a region and a single undo step.
([Recording](manual/07-recording.md))

## Routing and sync

- Per-channel **input and output routing**: the stereo main mix with pan,
  or direct to any device output, for example an external mixer.
  ([Audio routing](manual/04-main-window.md#audio-routing))
- Mute, solo, gain, pan and level meters per channel, plus master volume and
  meter.
- **Ableton Link:** syncs tempo and transport with other Link software on
  the network.
- A **DSP load meter** in the header.

## Import and export

- **Import:** WAV, AIFF, FLAC, Ogg Vorbis and MP3, by drag and drop from
  your file manager or the built-in File Browser.
- **Export** renders offline, faster than real time, to **WAV, FLAC, AIFF,
  Ogg Vorbis or MP3** (LAME built in). Mixes can be mono or stereo, and
  stems multichannel or one file per channel. A single clip exports from its
  context menu. ([Exporting audio](manual/09-export.md))
- **Open project format:** a `.audium` project is a folder holding a plain
  JSON project file and its audio.

## AI agents and scripting

([The command line and AI agents](manual/11-cli-and-agents.md))

- **MCP server** (`audionaut-mcp` on npm, also in the official MCP
  registry) with 22 tools: create a project, import, analyse, auto edit,
  assemble, split, create, retrim and clean up regions, place, move and
  remove clips, clip gain, fades, speed and stretch, stem separation, remove tracks and channels, export, project
  info, and `request_feature` / `report_bug`, which file GitHub issues.
  Setup for Claude Code, with the app and Node.js 18+ installed:
  `claude mcp add audionaut -- npx -y audionaut-mcp`
- **Edits land in the open document.** If the project is open in the app,
  each command runs inside the running app on the live state, unsaved
  changes included, and arrives as one undo step. The agent never
  overwrites your file, and saving stays with you.
- **The app is also a command line.** Every verb runs headless with a JSON
  result envelope, and positions are musical (bars and beats) by default.

## Platforms

- **macOS 13+ (Apple silicon):** a notarized DMG, or the Mac App Store. The
  app is sandboxed, so projects and audio for agents live in `~/Music`.
- **Windows (x64):** an installer. It is not code-signed yet, so SmartScreen
  warns on first run.
- **Linux (x86_64):** AppImage and .deb.

Downloads: [audionaut.app/download](https://audionaut.app/download).
Privacy: anonymous usage statistics are opt-in only
([Privacy](manual/12-privacy.md)).

## Not yet

Planned or under consideration, not available today:

- Plugin hosting (VST3 / AU / LV2), effects and automation envelopes
- MIDI
- Timeline markers and labels, import and export of label or region lists,
  and OSC cues
- Automatic crossfades when clips are dragged over each other
- Import from a menu (today it's drag and drop only)
- Speech-specific tools (silence or filler-word removal)
- Flatpak, and a code-signed Windows installer

Ideas and requests are welcome in
[GitHub Discussions](https://github.com/kvoltmer/Audionaut/discussions).
