# Exporting audio

Exports render offline — faster than realtime and independent of your audio
device. Everything you hear is in the bounce: clip gains, fades, crossfades
and the mix of all tracks.

## Exporting the arrangement

*File → Export Audio…* (**Cmd+Alt+B**). The dialog offers:

- **Format** — WAV, FLAC, AIFF or Ogg Vorbis
- **Sample rate**
- **Output channels**
- **Bit depth** (WAV, FLAC, AIFF) or **Quality** (Ogg Vorbis)

| Format | Kind | Bit depth | Notes |
|---|---|---|---|
| **WAV** | lossless | 16, 24 or 32 | |
| **FLAC** | lossless, compressed | 16 or 24 | noticeably smaller than WAV; at most 8 channels per file |
| **AIFF** | lossless | 16 or 24 | |
| **Ogg Vorbis** | lossy | — | Quality from 64 to 500 kbps (default 192 kbps); at most 8 channels per file |

FLAC and AIFF hold exactly the same audio as a WAV of the same bit depth. Ogg
Vorbis gives up a little detail for much smaller files, which suits sharing a
mix more than archiving it. The kbps figures are approximate, since Vorbis
varies its bit rate with the material. For a multi-channel export wider than
eight channels in FLAC or Ogg Vorbis, choose multi-mono (one file per channel)
or a different format.

**Mono** and **stereo** exports render the mix as you hear it, including
each channel's [output routing](04-main-window.md#audio-routing): channels
routed directly to outputs 1 or 2 land on those file channels unpanned, and
channels routed to higher outputs are left out. **Multi-channel** and
**multi-mono** exports are stems — every channel of the arrangement goes to
its own file channel (or file), regardless of routing. Multi-mono files are
named after the one you choose, numbered per channel: `mix.flac` becomes
`mix-01.flac`, `mix-02.flac`, and so on.

## Exporting a single clip

Right-click a clip and choose **Export…**. The clip is bounced by itself —
with its own gains and fades applied, so it sounds exactly as it does in the
arrangement.

Name the file `.wav`, `.flac`, `.aiff` or `.ogg` to pick the format. A FLAC
or AIFF from a 32-bit float source is written at 24 bits, and an Ogg Vorbis
file uses the default quality.

## Exporting from the command line

The `export` verb of `audionaut-cli` renders the same way. The format follows
the output file's extension (`-o mix.wav`, `.flac`, `.aiff` or `.ogg`), and an
Ogg Vorbis export takes `--quality 0`–`10` (6, about 192 kbps, by default)
instead of `--bit-depth`. It adds a few scripted conveniences: a start/length
window, one-mono-file-per-channel (`--multi-mono`), and bouncing a named region
(always dry — a region is raw material; gains and fades belong to clips). See
[The command line and AI agents](11-cli-and-agents.md).
