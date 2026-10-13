# Introduction

Audionaut is a desktop audio editor for multitrack recordings. It is built for
working with material that arrives as a set of parallel audio files — stems,
multitrack session exports, field recordings — and turning them into an
arrangement: cutting, naming, rearranging, fading and bouncing.

What sets it apart:

- **Multitrack-first.** A track holds any number of channels playing in
  lockstep; a clip on the timeline moves all of a track's channels together.
- **Musical analysis built in.** Audionaut can analyse audio (segment
  boundaries, onsets, beats) and use the results to cut material into
  musically aligned segments and assemble new arrangements from them
  automatically.
- **Region-based editing.** Named regions describe slices of your source
  material; the timeline places them. The same region can appear many times
  in an arrangement without duplicating audio.
- **Agent- and script-friendly.** Everything the timeline can do is also
  available headlessly through `audionaut-cli` and an MCP server, so scripts
  and AI assistants can edit projects alongside you — the app picks up
  outside changes automatically.
- **Tempo-aware.** Projects carry a tempo; positions are musical (bars and
  beats), and Ableton Link support lets the transport sync with other
  software.

## Platforms

Audionaut runs on **macOS**, **Windows** and **Linux**.

## Licence

Audionaut is dual-licensed: free software under the **GPLv3**, with a
commercial licence available. See [LICENSE.md](../../LICENSE.md).
