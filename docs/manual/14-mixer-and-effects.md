# The mixer, effects and plugins

The mixer sits along the bottom of the middle area, under the arrangement.
*View → Mixer* (**Cmd+Shift+M**) opens and closes it, and the **Mixer** bar
at its top is a handle: drag it up for more room, down to close the mixer.
What is shown is remembered between sessions.

## Strips

Every track is a group: a band in the track's colour with the track's name,
and one strip per channel below it. Top to bottom a group holds

- **Input and output** for each channel, the same routing as in the
  arrangement's channel strips ([Audio routing](04-main-window.md#audio-routing)).
- **Inserts**: the track's effect slots. Inserts are per track and process
  all of its channels at once, so they appear once in the group, not in
  every strip.
- **Sends**: one knob per aux bus in every strip (see below).
- **Pan** for each channel.
- **Record, monitor, solo and mute**, two rows of keys as in the
  arrangement's strips.
- The **fader** with its meter, and the channel's name.

The narrow bar at the left of the mixer shows or hides each of these
sections for the whole mixer; the fader, meter and name always show. Hiding
the sections you do not need keeps the mixer low.

To the right of the scrolling strips, pinned in place, stands the **bus
section**: a strip per aux bus and the **Main** strip. *View → Aux Buses*
shows or hides the aux strips. A bus strip has its inserts, its pan (the
Main strip's is a balance), solo and mute (the Main strip has mute only),
a fader with a stereo meter and its name; double-click an aux bus's name to
rename it, right-click for *Rename* and *Remove*. *Create → Aux Bus* adds
one.

Every move in the mixer is one undo step: a fader or pan drag is a step from
where it started to where it ended, a key press is a step by itself.

## Sends and aux buses

An aux bus is a stereo return into the main mix with its own insert chain,
for a shared reverb or delay, a parallel compressor, a headphone mix. Every
project starts with two.

Each channel strip shows a knob for every aux bus, named after the bus. The
knob sits at silence until you turn it; the first movement creates the send
as part of that same undo step (undo takes it away again). Double-click
returns the knob to silence. Right-click for **pre-fader** (the send then
follows the pan but not the fader, for a monitor mix), **mute** and
**remove**. Sends follow the strip's pan, so a channel panned left arrives
left in the aux bus too.

### Solo

The mixer has two solo groups that do not interfere with each other:

- **Channel solo** is solo-in-place among the channels. The other channels
  fall silent; the aux returns stay audible. With post-fader sends a return
  then carries only what the soloed channels send, so you hear the soloed
  channel with its effects.
- **Aux solo** is the other way round. The dry main mix and the other aux
  returns fall silent, the channels keep playing into the soloed bus, and
  you hear that bus alone.

A strip whose group has a solo active dims its mute key, as the mute is
overruled meanwhile.

## Inserts

Every track, every aux bus and the Main bus has a chain of insert slots.
Click an empty slot to add an effect from the menu: the built-ins first by
category, then your plugins by manufacturer. The new insert's editor opens
at once. Click a used slot to open its editor again; right-click it for **Edit…**, **Bypass** or **Enable**, **Replace**
(with another type, the settings of the old one are lost), **Move Up** and
**Move Down**, and **Remove**. Drag a slot within its chain to reorder. A
chain longer than the visible rows scrolls with the mouse wheel.

Bypass is click-free, and a bypassed slot shows its name dimmed. Each insert
is processed on every channel of its track (a stereo track runs the effect
in stereo); the signal reaches the inserts after the channel's input
routing and before the sends and the pan.

The latency an effect adds is compensated at every summing point: a track
with a latent insert is not shifted against the others, exports stay on the
grid, Link peers stay in time and recorded takes land where they were
played. The whole chain is heard the same in playback and export.

### The built-in effects

- **Trim**: gain in dB.
- **EQ**: low and high shelves, a mid band, and a high-pass filter that can
  be switched in.
- **Compressor**: threshold, ratio, attack, release, look-ahead and make-up
  gain, with a gain-reduction meter in its editor. On a stereo track both
  channels are reduced alike.
- **Delay**: a feedback delay with time, feedback and mix.

### Effect editors

An effect's editor opens in a small window that floats above the main
window and hides while another application is in front. Several can be
open at once; the mixer's shortcuts keep working with an editor in front,
undo included. Escape or the close button closes an editor; one whose slot
is removed closes itself.

The window is the effect: nothing but its controls, under a title that
names the track or bus and the effect. Bypass stays in the slot's menu in
the mixer; a bypassed effect's editor is dimmed.

A built-in effect's editor is a grid of dials, each with its name above and
its value below. Drag a dial, or turn the mouse wheel over it; double-click
the value to type one in; double-click the dial to return to its default. A
dial drag is one undo step from the value before to the value after. The
compressor shows its gain reduction in a strip under the dials.

A plugin's editor is the plugin's own interface. The window takes the size
the plugin asks for and follows when the plugin changes it; a plugin whose
interface can be resized makes the window resizable too. A plugin without
an interface of its own gets a plain list of its parameters.

## Plugins

Audionaut hosts the plugin formats native to each platform:

| Platform | Formats |
|---|---|
| macOS | Audio Units (AUv2 and AUv3) |
| Windows | VST3 |
| Linux | VST3 and LV2 |

Only effect plugins are used as inserts; instruments are listed but not
hosted. On macOS the application runs in the system's sandbox, on the Mac
App Store and in the downloaded build alike. Audio Units that are not
marked sandbox-safe cannot be opened inside it, so Audionaut runs those,
and every AUv3, in a process of their own: macOS's own plugin hosting
service, the same one Logic uses. Such a plugin works like any other, its
window included. It takes a moment longer to appear after you add it (the
slot passes the signal through until it is there), and a plugin that
crashes takes down only that service, not Audionaut. The line under the
Manage Plugins list says how many of your plugins run this way.

### Manage Plugins

*Options → Manage Plugins…* (also at the end of every insert menu) opens
the list of plugins Audionaut knows. The list starts empty: choose the
**Options** button and **Scan for new or updated … plug-ins** to look
through the platform's plugin folders. Each plugin file is examined in a
separate process, so a plugin that crashes while it is examined takes
nothing down with it; it is skipped from then on, and the line under the
list counts such files. The same menu removes entries you do not want to
see and sorts the list. Scanning runs while you work; the plugins appear in
the insert menus as soon as the scan found them.

The list is kept in Audionaut's data folder, not in the project, so it is
scanned once per machine. The command line shares it (`scan-plugins` and
`list-effects` in [The command line and AI agents](11-cli-and-agents.md)).

### Plugins in a project

A project remembers each plugin insert by its format and identity, together
with the plugin's own settings, and it also notes the plugin's name and
manufacturer. Open the project on a machine that has the same plugin in
another format (a VST3 where the project was saved with the Audio Unit),
and the slot opens with that plugin, settings handed over; the project is
written with the new format on the next save.

A plugin the machine does not have at all stays in the project as a
**missing** insert: a dimmed row in its chain with a marker, the plugin's
name and manufacturer as its tooltip (and the reason, when the plugin is
installed but cannot run here). It is not processed and takes no
slot index, so agents and the command line count the remaining inserts as
before. Saving the project keeps it, settings included, for the machine
that has the plugin; right-click **Remove** to drop it (undoable). A plugin
that is installed but will not load here, or refuses the track's channel
count, keeps its row with the same marker and the reason as its tooltip,
and its slot passes the signal through meanwhile.

Agents and the command line address plugins by type id (`au:…`, `vst3:…`,
`lv2:…`) and set their parameters normalised from 0 to 1; see
[The command line and AI agents](11-cli-and-agents.md).
