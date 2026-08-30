# Quick start

This walkthrough takes a set of audio files to a finished bounce.

## 1. Create a project

*File → New Project…* (**Cmd+N**). An Audionaut project is saved as a
`.audium` package — a folder that contains the project file and all audio
media, so it moves between machines (and between the app and the command
line) as one unit.

## 2. Import audio

Drag audio files into the arrangement — either from your system's file
manager, or from Audionaut's own **File Browser** (*View → Open File
Browser*), which starts in your Music folder and supports multi-selection.

Imported files land on a track. Files imported together become the channels
of one track: an eight-stem export becomes one track with eight channels that
always play in sync.

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
WAV file — choose sample rate, bit depth and channel count in the dialog.
Details in [Exporting audio](09-export.md).

## Saving

**Cmd+S** saves; Audionaut also autosaves after edits, and if a project is
changed on disk by something else (for example the command-line tool), the
app reloads it automatically.
