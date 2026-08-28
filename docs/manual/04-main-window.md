# The main window

<!-- screenshot: annotated main window overview -->

The window has three areas: the **header bar** across the top, the **middle
area** with channel strips and the arrangement timeline, and the **right
panel** with regions and playlists. A separate floating **File Browser**
window is available from the *View* menu.

## Header bar

The header carries everything global:

- **Link** — enables Ableton Link, syncing tempo and transport with other
  Link-enabled software on your network.
- **Tempo** — the project tempo in BPM. All bar/beat positions derive from it.
- **Position** — the transport position in bars, beats and ticks; editable.
- **Play / Stop / Record / Loop** — the transport. **Space** toggles
  play/stop. Record arms the transport for recording (see
  [Recording](07-recording.md)).
- **Master meter and volume** — a stereo level meter and the master output
  gain.
- A button to show or hide the right panel.

## Channel strips

Each track shows its channels as strips on the left of the arrangement. Per
channel:

- **M** — mute, **S** — solo
- **Record** — arms the channel for recording
- **Monitor** — routes the channel's input to the output while armed
- a level meter, gain and pan controls

Channel height is adjustable (micro / small / medium / large / huge) to fit
many channels on screen — set it per channel with the height combo on the
strip, or for the whole track by **right-clicking the track header** and
choosing a size from the **Waveform Size** submenu. The same track-header
menu also holds the **Show Analysis** toggles (see
[Analysis and Auto Edit](08-analysis-and-auto-edit.md)).

Right-clicking a channel offers **Copy selected channel(s) to new track**.

## Arrangement

The timeline. Tracks run horizontally; clips are drawn with their waveforms
and can be dragged, trimmed and split. A bar/beat grid underlies everything
(snapping can be toggled), and analysis results — segment boundaries,
beats — can be displayed over the material.

Navigation: **Cmd++** / **Cmd+-** zoom, **←** / **→** page left and right,
*View → Follow Transport* (**Ctrl+F**) keeps the view on the playhead.

## Right panel

Two stacked lists, divided by a draggable resizer:

- **Regions** — every named region in the project. This is the pool of
  material you can place on the timeline.
- **Playlists** — the sequence of clips per track.

## File Browser

*View → Open File Browser* opens a floating file tree (starting in your
Music folder). Select one or more audio files and drag them into the
arrangement to import them.
