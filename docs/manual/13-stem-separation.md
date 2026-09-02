# Stem separation

Audionaut can split a mixed recording into its instruments: select a clip,
choose *Edit → Separate Stems…*, and four new tracks appear — **Drums**,
**Bass**, **Other** and **Vocals** — each carrying that part of the clip,
lined up with the original.

Separation is powered by [Demucs](https://github.com/facebookresearch/demucs)
(Meta AI Research's *htdemucs* model) running on your own computer through
[demucs.cpp](https://github.com/sevagh/demucs.cpp). Nothing is uploaded
anywhere.

## The model

The first time you separate stems, Audionaut asks to download the model — a
one-time download of about 80 MB. It is kept in a *Models* folder in your
user application-data directory (*Settings → Separation* shows the exact
location, with a *Show Folder* button), and verified against a checksum
before it is used.

*Settings → Separation* also lets you download the model ahead of time,
remove it again, choose whether the original track is muted after a
separation, and pick how many **threads** the separator may use. Each
thread works on its own stretch of the clip, so more threads finish sooner
at the cost of more memory; the default is the number of processor cores.

If your computer is offline, copy the file `ggml-model-htdemucs-4s-f16.bin`
into the *Models* folder by hand — Audionaut checks it the same way.

## Separating a clip

1. Stop playback and select exactly one clip. The clip's track must be mono
   or stereo; clips up to ten minutes long can be separated.
2. Choose *Edit → Separate Stems…*.
3. A progress window shows the render, the separation and the writing of
   the stems. **Cancel** stops the job and leaves the project untouched — it
   can take a few seconds to react.

Separation runs on the processor and takes a while: expect roughly the
length of the clip per thread on a recent desktop machine, longer on
laptops. The application is busy for the duration.

When it finishes, four tracks are added at the bottom of the track list,
named after the clip — *Take 3 - Drums*, *Take 3 - Bass* and so on. Each
holds one clip — mono for a mono source, stereo for a stereo one —
starting where the source clip starts (a fade-in that
reaches ahead of the clip is included, so the stems start that much
earlier). The original track is muted, so what plays afterwards is the
separation; unmute it to compare, or turn this off in *Settings →
Separation* (*Mute the original track after separating*).

The whole operation is a single edit: **Cmd+Z** removes all four tracks
again. The stem files stay in the project's *Media/Audio* folder until the
project is saved.

The stems are rendered at 44.1 kHz, the model's rate, whatever the rate of
the source material; Audionaut resamples them on playback like any other
file.

## From an agent

Agents talking to Audionaut over MCP have the same command as the
`separate_stems` tool: it takes the project and a track (and optionally a
clip), and reports the four tracks it created. It needs the model to have
been downloaded from the app once.
