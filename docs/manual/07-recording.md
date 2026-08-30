# Recording

Audionaut records onto armed channels while the transport plays.

## Setting up

1. Pick your **input device** in *Settings → Audio* and enable the input
   channels you need.
2. Choose which input feeds each channel with the **In** combo on its strip.
   By default a channel records the input with the same number as its
   position in the track; pick any other input to record, say, a single
   microphone onto several tracks or an interface's input 5 onto a mono
   track (see [Audio routing](04-main-window.md#audio-routing)).
3. **Arm** the channels to record with the **Record** button on each channel
   strip. An armed channel's meter shows the incoming level. A channel whose
   input the current device does not provide cannot be armed.
4. Optionally enable **Monitor** on a channel to hear its input through the
   channel's output while armed.

## Recording a take

Arm the transport with the header **Record** button, then press **Play**
(**Space**) — recording starts at the playhead. Pressing the record button
while already playing also starts recording on the fly. Stop with **Space**.

Each armed channel records to its own audio file inside the project package;
the take appears on the timeline as a new numbered take region. A take is a
single undoable action — **Cmd+Z** removes it entirely.

While a clip is still recording it cannot be moved or trimmed; once
recording stops it behaves like any other clip.
