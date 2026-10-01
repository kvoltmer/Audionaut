# Exporting audio

Exports render offline — faster than realtime and independent of your audio
device. Everything you hear is in the bounce: clip gains, fades, crossfades
and the mix of all tracks.

## Exporting the arrangement

*File → Export Audio…* (**Cmd+Alt+B**). The dialog offers:

- **Format** — WAV or FLAC
- **Sample rate**
- **Output channels**
- **Bit depth**

**WAV** takes 16, 24 or 32 bits. **FLAC** is lossless, so it holds exactly
the same audio as a WAV of the same bit depth, in a noticeably smaller file.
It takes 16 or 24 bits, and one FLAC file holds at most eight channels. For a
wider multi-channel export, choose multi-mono (one file per channel) or WAV.

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

Name the file `.wav` or `.flac` to pick the format. A FLAC from a 32-bit
float source is written at 24 bits.

## Exporting from the command line

The `export` verb of `audionaut-cli` renders the same way. The format follows
the output file's extension (`-o mix.wav` or `-o mix.flac`). It adds a few
scripted conveniences: a start/length window, one-mono-file-per-channel
(`--multi-mono`), and bouncing a named region (always dry — a region is raw
material; gains and fades belong to clips). See
[The command line and AI agents](11-cli-and-agents.md).
