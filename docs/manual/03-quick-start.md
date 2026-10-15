# Quick start

This walkthrough takes a set of audio files to a finished bounce.

## 1. Create a project

*File → New Project…* (**Cmd+N**). An Audionaut project is saved as a
`.audium` package — a folder that contains the project file and all audio
media, so it moves between machines (and between the app and the command
line) as one unit.

## 2. Import audio

Choose **File → Import...** (⇧⌘I) and pick one or more audio files. They land
on new tracks starting at the playhead. When you pick several files, the
*Multiple files* option in the dialog decides how they are laid out:

- **Separate tracks** — one track per file.
- **One track, stacked channels** — the files become the channels of one
  clip: an eight-stem export becomes one track with eight channels that
  always play in sync.
- **One track, back to back** — the files follow each other on one track.

Audionaut remembers your choice. A single *Undo* takes the whole import back.

To add audio to a track you already have, **right-click an empty spot on the
track** and choose **Import Audio...**. The files land on that track where you
clicked (snapped to the grid when snapping is on), stacked as channels of one
clip or back to back.

You can also drag audio files into the arrangement — either from your
system's file manager, or from Audionaut's own **File Browser** (*View → Open
File Browser*), which starts in your Music folder and supports
multi-selection. Files dragged in together become the channels of one track.

If automatic analysis is enabled (*Settings → Analysis*), Audionaut starts
analysing new material in the background right away — this powers the Auto
Edit features later.

## 3. Play

Press **Space** to start and stop playback. The header bar has the transport:
play, stop, record, loop, the tempo, and the position display in bars and
beats. Master volume and a level meter sit on the right.

## 4. Arrange

- Drag clips along the timeline to move them.
- Put the playhead where you want to cut and use *Edit → Split* (**Cmd+E**).
- Select a time range and *Create → Create Region…* (**Cmd+R**) to name a
  section — named regions collect in the right-hand panel, ready to be
  placed again.
- **Cmd+D** duplicates the selected clip.

See [Editing](06-editing.md) for the full toolset, and
[Analysis and Auto Edit](08-analysis-and-auto-edit.md) for letting Audionaut
cut and arrange for you.

## 5. Export

*File → Export Audio…* (**Cmd+Alt+B**) renders the arrangement offline to a
WAV, FLAC, AIFF, Ogg Vorbis or MP3 file — choose the format, sample rate,
channel count and bit depth (or, for Ogg Vorbis and MP3, the bit rate) in the
dialog.
Details in [Exporting audio](09-export.md).

## Saving

**Cmd+S** saves; Audionaut also autosaves after edits, and if a project is
changed on disk by something else (for example the command-line tool), the
app reloads it automatically.
