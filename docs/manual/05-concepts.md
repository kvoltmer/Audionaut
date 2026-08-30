# Concepts: projects, tracks, regions and clips

Understanding four ideas makes everything else in Audionaut predictable.

## Projects

A project is a `.audium` **package**: a folder containing the project file
and a `Media/Audio/` directory with all audio files. Copy the package and
you have copied the whole project. The same package is read and written by
the desktop app and the command-line tool.

Audionaut **autosaves** after edits, and **reloads automatically** when the
project changes on disk — so external tools (like `audionaut-cli` or an AI
agent) can edit a project while it is open.

## Tracks and channels

A **track** is a group of channels that play in lockstep — for example the
eight stems of a mix, imported together. Each channel has its own gain, pan,
mute/solo and record arm, but clips on the timeline always move all of a
track's channels together.

## Regions

A **region** is a named slice of a track's source material — a start and end
within the audio, plus a name. Regions live in a per-track pool (the Regions
list in the right panel) and are created by you (*Create Region…*, splitting)
or by the analysis features (segmentation).

Regions don't occupy the timeline by themselves; they are the *vocabulary*
an arrangement is built from.

## Clips

A **clip** is a placement of a region on the timeline: *this region, at this
bar*. Key consequences:

- A clip has **no length of its own** — it is exactly as long as its region.
  Trimming a region retrims **every** clip that uses it.
- The same region can be placed many times without duplicating audio.
- Per-clip properties are the *dynamics*: clip gain (per channel) and fades.
  Two clips of the same region can sound different.
- To trim just one clip independently, **split** it first — splitting mints
  fresh regions for the pieces.

Deleting a clip never deletes audio; deleting a region (*Edit → Delete
Unused Regions*) only removes pool entries no clip uses, and the audio files
always stay in the package.

## Musical time

Positions are musical: bars and beats at the project tempo (4/4). Bar 1 is
the start of the timeline. Changing the tempo changes where every bar falls
in real time.
