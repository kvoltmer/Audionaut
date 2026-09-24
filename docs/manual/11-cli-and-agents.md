# The command line and AI agents

Everything the timeline can do is also available headlessly. Two front doors:

- **`audionaut-cli`** — a console binary (built with the test CMake project;
  see the repository [README](../../README.md) for building it).
- **The app itself** — running the Audionaut binary with a verb executes it
  headlessly and quits, even while a GUI instance is open.

## Working on a project that is open in the app

You do not have to save, close, or otherwise get out of the way before handing
a project to an agent.

While Audionaut has a project open it serves commands for it, and a verb
addressed at that project is handed over instead of going to the file. The
command then acts on the document **as you currently see it**, unsaved changes
included. Its result arrives as a single entry in your undo history — labelled
after the verb, with the window title marked *Edited By Agent* — so ⌘Z takes it
back like any other edit. Nothing is written to `Project.json`; saving stays
yours to do.

Your own edits take precedence. If you change the project while a command is
running, its result is dropped and the agent is told to run it again rather
than have your work replaced. Commands are refused while you are recording,
and exports are refused while the transport is playing.

With no app holding the project, verbs read and write the project file exactly
as they always have.

If the app is holding a project but a command cannot reach it, the command
**fails** rather than falling back to the file — the file is missing whatever
you have not saved, and quietly writing over it is the one outcome worth
refusing. Set `AUDIONAUT_AGENT_ROUTING=0` to work on the file deliberately.

Two files inside a package belong to the app and should be left alone:
`Autosave.json`, its crash-recovery snapshot, and `Host.json`, the marker
saying which process is holding the project.

One limit worth knowing: a command running inside the app runs inside its
sandbox, which can write to your Music folder and to places you have picked in
a file dialog, but not anywhere else. An export elsewhere is refused with
`sandbox_denied`; quit the app to export there with the command line instead.

## Verbs

```
info            project summary (tempo, tracks, clips) or raw project JSON
create          new empty project
import          add audio files at a position
export          offline bounce (whole project, a window, or one region)
analyze         run Essentia analysis and cache the results
auto-edit       segment a clip at analysed boundaries
assemble        build an arrangement from a track's regions
split           split clips at a timeline position
create-region   name a region from a timeline range
set-region      rename and/or retrim a region
remove-clip     remove clip(s) from the timeline
move-clip       move a clip to a new position
place-clip      place an existing region on the timeline
cleanup-regions delete every region no clip uses
clip-gain       set a clip's gain (linear or dB)
clip-fades      set a clip's fade lengths, offsets and curves
clip-speed      set a clip's speed (ratio, semitones or target length)
                and mode (--mode repitch|stretch, pitch-preserving)
remove-track    remove a whole track (channels, clips and regions)
remove-channel  remove one channel from a track
separate        split a clip into Drums/Bass/Other/Vocals tracks (Demucs)
```

Run `audionaut-cli --help` for each verb's options. Positions and durations
are musical by default — `--unit bars|beats|seconds|clocks`, bars and beats
1-based — so "split at bar 23" is literally `split song.audium --at 23`.

## Scripting contract

Every verb takes `--json`, which prints exactly one machine-readable result
envelope on stdout — `{"ok": true, "result": …}` or `{"ok": false,
"error": {"code": …, "message": …}}` — with all logging on stderr. Exit
codes: `0` success, `1` failed, `2` usage error, `3` feature unavailable in
this build.

Example session:

```
audionaut-cli split      song.audium --at 23
audionaut-cli create-region song.audium --name chorus --start 17 --end 25
audionaut-cli place-clip song.audium --region chorus --at 33
audionaut-cli clip-fades song.audium --region chorus --fade-in 1 --unit beats
audionaut-cli export     song.audium -o mix.wav --sample-rate 48000
```

## AI agents (MCP)

`Tools/audionaut-mcp` is an MCP server exposing every verb as a tool, so AI
assistants (Claude, and any other MCP-capable agent) can inspect and edit
projects conversationally. Registration instructions are in
[`Tools/audionaut-mcp/README.md`](../../Tools/audionaut-mcp/README.md).

## Privacy

CLI invocations follow the desktop app's analytics consent — see
[Privacy](12-privacy.md). `AUDIONAUT_DISABLE_ANALYTICS=1` switches CLI
reporting off regardless, which is recommended for CI.
