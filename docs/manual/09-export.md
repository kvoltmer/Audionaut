# Exporting audio

Exports render offline — faster than realtime and independent of your audio
device. Everything you hear is in the bounce: clip gains, fades, crossfades
and the mix of all tracks.

## Exporting the arrangement

*File → Export Audio…* (**Cmd+Alt+B**). The dialog offers:

- **Sample rate**
- **Output channels**
- **Bit depth**

The result is a WAV file.

**Mono** and **stereo** exports render the mix as you hear it, including
each channel's [output routing](04-main-window.md#audio-routing): channels
routed directly to outputs 1 or 2 land on those file channels unpanned, and
channels routed to higher outputs are left out. **Multi-channel** and
**multi-mono** exports are stems — every channel of the arrangement goes to
its own file channel (or file), regardless of routing.

## Exporting a single clip

Right-click a clip and choose **Export…**. The clip is bounced by itself —
with its own gains and fades applied, so it sounds exactly as it does in the
arrangement.

## Exporting from the command line

The `export` verb of `audionaut-cli` renders the same way and adds a few
scripted conveniences: a start/length window, one-mono-file-per-channel
(`--multi-mono`), and bouncing a named region (always dry — a region is raw
material; gains and fades belong to clips). See
[The command line and AI agents](11-cli-and-agents.md).
