# Insert and send effects: signal design and implementation plan

## 1. Context

Audionaut has no effects. [`features.md`](../../features.md) lists "Plugin hosting (VST3 / AU /
LV2), effects and automation envelopes" under *Not yet*.

The mixer is one flat array of mono bus channels (`MAX_AUDIO_CHANNELS`,
[`PlaybackDefines.h`](../../../Audionaut/Source/Engine/Playback/PlaybackDefines.h)). Every
`AudioChannel` of every `AudioTrack` owns one bus channel at `track.getChannelOffset() + channelIndex`
([`AudioChannel.cpp:114`](../../../Audionaut/Source/Engine/Channel/AudioChannel.cpp#L114)), voices
sum into their bus channel ([`Playback.h:95`](../../../Audionaut/Source/Engine/Playback/Playback.h#L95)),
and [`AudioBusRenderer::process`](../../../Audionaut/Source/Engine/Playback/AudioBusRenderer.h#L54)
applies gain (mute/solo folded in), pan into a stereo main mix or a direct device output, then
master gain. All mixer state lives in the renderer and is written only through `LockFreeCommander`
lambdas.

This plan adds **insert effects** (per track, one chain that sees all bus channels of the track at once,
plus aux and master chains) and **send effects** (per-channel aux sends into dedicated FX buses
that return to the main mix), in phases, and a **multithreaded renderer** that runs tracks and
aux buses in parallel inside the audio callback without adding latency (2.9). The renderer comes
first: it needs no effect code, fixes the stretched-track overruns on its own, and leaves the
renderer in the job form the effect phases and track-to-track recording (2.10) build on.

Decisions taken (2026-10-25):

- Built-in effects first (juce_dsp, already a project module, no new dependencies). VST3/AU
  hosting implements the same interface in a later phase.
- Revised 2026-11-13, and again 2026-11-14 (PR #257 ships the built-ins in release builds after
  all): the built-in effects (Trim, EQ, Compressor, Delay) stay, registered in every build, and
  plugin hosting (phase 7) adds the plugins beside them. Phase 6 stays dropped: effects
  that sound good but not exceptional would hurt Audionaut more than missing ones: a set of 23
  vendored Airwindows effects was built and removed again (PR #257), and phase 6 is dropped.
- Inserts per track, processing all bus channels of the track together (stereo effects on a stereo
  track). The first design also had mono inserts per bus channel, console style; they were dropped
  on 2026-11-07 (phase 5) and removed from the engine in PR #254.
- Send destinations are dedicated FX buses shown as strips in the channel view, not tracks. The
  master bus gets an insert chain too.

Tracktion Engine (GPL3/commercial) was studied for ideas only. Nothing is ported; section 2.8
lists where this design differs and why.

## 2. Signal design

### 2.1 Topology (fixed, not a general graph)

```
hardware input (monitor) ─┐
voices (clips) ───────────┴─► bus channel k ─────────────────────────┐
                                                          │ for each track t: bus channels [off..off+n)
                                           [track inserts t] (n-channel block, in place)
                                                          │
                                         PDC delay (pre-fader align, n-channel, per track)
                                                          │
                                 ┌── pre-fader send taps ──┤
                                 │                     channel gain (mute/solo) ─► channel meter
                                 │                        │
                                 │                  pan (mono→stereo)
                                 │                        │
                                 └─► post-fader send taps ┤
                                                          ├─► main mix (stereo)      or   direct out (post-gain, unpanned)
aux bus a (stereo): Σ sends ─► [aux inserts a] ─► PDC ─► return gain/balance/mute ─► + main mix
main mix ─► PDC (dry delay) ─► + aux returns ─► [master inserts] ─► master gain ─► master meter ─► device out 0/1
```

- **Track insert**: n-channel chain over the contiguous bus channels of one track (n = track channel
  count), before the fader. The only insert stage on the tracks; there are no per-channel inserts.
  Contiguity holds because
  `commitChannelData()` places bus channels at `channelOffset + index`. Processed with
  `AudioBlock::getSubsetChannelBlock(offset, n)` in place, no copy.
- **Sends** are per bus channel: each bus channel has 0..N entries `{auxId, level, preFader, mute}`. A send is
  *post-pan*: the bus channel's mono signal is panned with the bus channel's pan into the stereo aux. Post-fader
  sends tap the bus channel's `stereoBlock` right after `panners[i].process`
  ([`AudioBusRenderer.h:131`](../../../Audionaut/Source/Engine/Playback/AudioBusRenderer.h#L131)),
  exactly what the main mix gets, scaled. Pre-fader sends tap before gain with a stateless gain
  pair from the same balanced pan rule (`juce::dsp::Panner` default, `Rule::balanced`), so they
  diverge from the main pan only during a pan move. Sends follow mute/solo unless pre-fader.
- **Aux bus**: stereo, first-class object (`AuxBus`): stable id, name, insert chain, return gain,
  balance, mute, solo. v1 routes only to the main mix. No aux→aux sends in v1, so the topology is
  acyclic by construction and needs no cycle detection. Aux→aux with a topological order is a
  later extension.
- **Two aux buses by default** (Klaus 2026-10-31), like the aux sends of a console: every project
  has "Aux 1" (id 1) and "Aux 2" (id 2) from the start. `AuxBusContainer` creates them for a new
  project and when a project without an `"aux_buses"` key loads, so older projects gain them on
  open without a file change until saved. They are ordinary buses otherwise: renamable, removable,
  more can be added. A bus channel without a send entry for an aux sends nothing to it; the strip
  shows one send control per existing aux bus, so the two defaults appear as two rotaries on every
  channel.
- **Master inserts** sit before master gain, so the master fader stays the last stage.
- **Direct outs** (`outputChannel >= 0`) keep today's semantics (post-gain, unpanned, no master),
  get PDC alignment (2.4), and can still feed sends.
- **Stem export** (`config->numChannels > 2`,
  [`PlayListScheduler.cpp:756`](../../../Audionaut/Source/Engine/PlayList/PlayListScheduler.cpp#L756))
  copies bus channels post-inserts, post-gain as today; aux returns are not in stems (standard DAW
  behaviour, to be documented).
- **Monitoring** input is added to the bus channel before the inserts, so monitored input is heard through
  the inserts while the recorder still captures dry input (it reads `inputBlock`). Free.
- **Clip bounce** (`bouncePlayListItem`) bypasses the bus today and stays dry. Unchanged.

### 2.2 The `Effect` contract (one interface for built-ins and hosted plugins)

```cpp
struct EffectParameterInfo { juce::String id, name, unit; float min, max, def; bool boolean; };

class Effect {
public:
    virtual ~Effect() = default;
    virtual const EffectTypeInfo& getType() const = 0;              // type id ("builtin:eq3"), name, supportsChannels(n)
    // message thread only; the planner guarantees no live plan uses a different spec
    virtual void prepare (double sampleRate, int maxBlockSize, int numChannels) = 0;
    virtual void release() = 0;
    virtual void reset() noexcept = 0;                                // clear delay lines etc., audio thread safe
    // audio thread: in place, numChannels == prepared, numSamples <= maxBlockSize
    virtual void process (juce::dsp::AudioBlock<float>& block) noexcept = 0;
    virtual int getLatencySamples() const noexcept { return 0; }
    virtual double getTailSeconds() const noexcept { return 0.0; }
    // parameters: stable ids; values are atomics inside the effect; smoothing is the effect's job
    virtual std::span<const EffectParameterInfo> getParameters() const = 0;
    virtual float getParameter (int index) const noexcept = 0;
    virtual void setParameter (int index, float value) noexcept = 0;  // message thread / CLI, RT-safe store
    virtual json getState() const;      // default {paramId: value}; MUST be deterministic (fixed order, rounded)
    virtual void setState (const json&); // inverse
};
```

- Channel support: `EffectTypeInfo::supportsChannels(int n)`. Track, aux and master inserts need
  n-channel (a track's chain runs at the track's channel count). Built-ins support 1..n generically.
- `EffectSlot` (`shared_ptr`-owned): `{type, enabled, shared_ptr<Effect>, bypass crossfade +
  bypass delay runtime}`. `InsertChain` = vector of `shared_ptr<EffectSlot>` + JSON in/out +
  add/remove/move/enable. One `InsertChain` type serves track, aux and master.
- `EffectRegistry`: engine service injected via
  [`AudiumFactory`](../../../Audionaut/Source/Engine/Factory/AudiumFactory.cpp), **no statics**
  (several engines per process). Maps type id → factory + `EffectTypeInfo`. Built-ins register in
  the factory; the hosting phase adds a second source (`KnownPluginList`).
- Bypass is click-free and latency-neutral: the slot crossfades wet/dry over ~10 ms on toggle,
  and when the effect reports latency the dry path runs through a delay of the same length.
- Deterministic `getState()` matters:
  [`AudioTrack::readFromJson`](../../../Audionaut/Source/Engine/Group/AudioTrack.cpp#L168)
  early-returns when the JSON equals the current state; non-deterministic state would defeat
  that and re-commit every channel on each undo.

Built-ins (juce_dsp): Trim (gain + polarity), EQ (3 band IIR + HPF), Compressor, Limiter, Gate,
Reverb, Delay, Chorus, Phaser. Phase 1 ships Trim, EQ, Compressor.

### 2.3 Threading: the immutable `MixPlan`

Today the renderer's structure is fixed; only values change through commander lambdas. Effects add
*structure*. Structure changes rebuild a plain-data **plan** on the message thread, handed to the
audio thread without locks; values keep flowing through atomics and commander lambdas.

```cpp
struct MixPlan {                       // immutable once published; built on the message thread
    struct Send { int aux; SendState* state; juce::SmoothedValue<float> level; bool preFader; };
    struct Channel { std::vector<Send> sends; int directOutDelay; };
    struct Track { int firstChannel, numChannels; std::vector<EffectSlot*> inserts; int preFaderDelay; };
    struct Aux   { AuxState* state; std::vector<EffectSlot*> inserts; int returnDelay; juce::dsp::Gain<float> gain; };
    std::vector<Channel> channels; std::vector<Track> tracks; std::vector<Aux> auxes; std::vector<EffectSlot*> master;
    int mainDryDelay, totalLatency; double sampleRate; int maxBlockSize;   // the spec it was prepared for
    std::vector<std::shared_ptr<EffectSlot>> keepAlive;   // every slot the plan references: survives model rebuilds
    std::vector<std::unique_ptr<PdcDelay>> trackDelays, channelDelays, auxDelays; PdcDelay mainDelay, directOutDelay; // carried over by id
    juce::AudioBuffer<float> auxBuffers;     // numAux * 2 channels, maxBlockSize
};
```

- **Ownership (critical)**: the plan holds `shared_ptr` to every slot it references
  (`keepAlive`). Whole-container loads with `rebuild=true` destroy every `AudioChannel` on the
  message thread
  ([`AudioTrackContainer.cpp:218`](../../../Audionaut/Source/Engine/Group/AudioTrackContainer.cpp#L218)
  → [`AudioTrack::cleanup`](../../../Audionaut/Source/Engine/Group/AudioTrack.cpp#L79)) while the
  audio thread still renders the published plan; model ownership alone would free live effects.
  Nothing is freed until the retired plan is deleted after `isPulled()`.
- **Publish**: `MixPlanContainer` wraps `LockFreeContainer<MixPlan*>` (capacity 1), modelled on
  [`VoiceSourceContainer`](../../../Audionaut/Source/Engine/AudioSources/VoiceSourceContainer.cpp).
  The audio thread pulls at the top of `AudioBusRenderer::process` (next to
  `lockFreeCommander->invoke()`) and, while stopped, through the existing `pullSnapshots()` path.
  The plan pointer is raw on the audio thread; nothing is ever freed there.
- **Retire**: the previous plan (and through its `keepAlive`, any removed effect) goes to a retired
  list and is deleted on the message thread once `isPulled()` is true (same 50 ms `juce::Timer`
  pattern as
  [`VoiceSourceContainer::releaseRetired`](../../../Audionaut/Source/Engine/AudioSources/VoiceSourceContainer.cpp#L124)).
  Slots present in both plans are the same objects, so a rebuild never re-prepares or resets them.
- **Rebuild triggers**, coalesced through a `juce::AsyncUpdater` on `MixerPlanner`: insert
  add/remove/move/enable, send add/remove, aux add/remove, channel layout change (hook
  `requestRebuild()` in
  [`publishNumAudioTrackChannels`](../../../Audionaut/Source/Engine/Group/AudioTrackContainer.cpp#L311),
  which every add/remove path calls; `commitChannelLayout` does not run on `addChannel` of the
  last track, [`AudioTrack.cpp:272`](../../../Audionaut/Source/Engine/Group/AudioTrack.cpp#L272)),
  a hosted plugin's latency change, and `prepare` with a new sample rate / larger block size.
- **Headless/CLI**: no message loop runs in `HeadlessEngineSession`, so an `AsyncUpdater` would
  never fire. `requestRebuild()` rebuilds synchronously when `HeadlessMode::isHeadless()` or an
  explicit per-engine flag is set (mirror of `setPumpsCommandsSynchronously`,
  [`AudioBusInterface.h:90`](../../../Audionaut/Source/Engine/Playback/AudioBusInterface.h#L90),
  for the scratch engine), and `bounceProject` flushes a pending rebuild next to
  `audioBusInterface->invokeCommands()`
  ([`PlayListScheduler.cpp:745`](../../../Audionaut/Source/Engine/PlayList/PlayListScheduler.cpp#L745)).
- **Values that do not rebuild**: effect parameters (atomics in the effect), send level/mute
  (`SendState` atomics read by the plan's `SmoothedValue` per block), aux return
  gain/balance/mute/solo (`AuxState` atomics), channel gain/pan/mute/solo (unchanged commander path).
- **prepare**: [`PlayListScheduler::prepareToPlay`](../../../Audionaut/Source/Engine/PlayList/PlayListScheduler.cpp#L72)
  calls `planner.prepare(sampleRate, blockSize)` before the bus. All callers run with the device
  stopped or bypassed (`audioDeviceAboutToStart`, `AudioExporter` after `setBypass(true)`, tests).
  The planner keeps its own `currentSpec`, seeded like
  [`AudioResource.cpp:50`](../../../Audionaut/Source/Engine/Resource/AudioResource.cpp#L50)
  (device if present, else 44.1 kHz / 512), prepares each new effect at slot creation, and on
  `prepare` only re-prepares effects when the sample rate changed or the block size grew (the
  exporter re-prepares per short last block,
  [`PlayListScheduler.cpp:947`](../../../Audionaut/Source/Engine/PlayList/PlayListScheduler.cpp#L947);
  resetting filter/delay state there would be audible). Safety net: the renderer skips a plan
  whose spec differs from its own.
- **Layout race**: `publishNumAudioTrackChannels` precedes `commitChannelLayout` on every path, so
  the published channel count and channel data are already up to one block apart. The plan is built
  from the same track vector; the renderer clamps: a `Track` with
  `firstChannel + numChannels > audioBusChannels` is skipped, bus channels iterate to
  `min(plan.channels.size(), audioBusChannels)`.
- **Audio thread invariants**: no allocation (aux buffers and delays sized at build), no locks,
  plan pointer read once per block, loops bounded by the plan's vectors and `MAX_AUDIO_CHANNELS`.
  New commander lambdas capture pointers/ints only (the existing `AudioChannelData` capture
  already exceeds libc++'s 24-byte small buffer and heap-allocates; do not make it worse).
- Two engines per process (scratch engine, tests) each own a planner, registry and plans; no
  static state.

### 2.4 Latency compensation (PDC), designed in, implemented in phase 4

Rule: *latency adds along a path; at every summing point each input is delayed to the longest*.
The topology has exactly three summing points, so the delays are computed arithmetically in
`MixerPlanner::build`, no graph search:

| Point | Inputs aligned | Delay inserted |
|---|---|---|
| Fader (per track) | each track's inserts | `preFaderDelay[t] = Lpre − trackLat[t]` (one n-channel delay over the track's bus channels), `Lpre = max over tracks` |
| Main mix + aux returns | dry main vs. each aux chain | `mainDryDelay = Laux = max auxLat[a]`, `returnDelay[a] = Laux − auxLat[a]` |
| Device output | main path vs. direct outs | direct outs delayed by `Laux + Lmaster` |

Total output latency `L = Lpre + Laux + Lmaster` is published by the planner as an atomic sample
count and consumed in three places:

- **Link sync.** The device callback already asks Link for the beat at the moment the buffer
  reaches the speaker (`hostTime + mOutputLatency`,
  [`LinkAudioDevice.cpp:55`](../../../Audionaut/Source/Engine/Link/LinkAudioDevice.cpp#L55)) and
  passes the same timestamp to `LinkEngine::audioCallback` for the start/stop/tempo stamps. PDC
  makes the whole mix `L` samples later, so the term becomes
  `hostTime + deviceOutputLatency + pdcLatency`. Without it Audionaut plays `L` behind every Link
  peer. Transport position, loop boundaries and the playhead display all derive from this
  compensated beat, so no separate display offset is needed.
- **Recording placement.** The recorder stamps a take with the beat computed for the block's
  *output* time; with PDC that is `L` later than when the input actually arrived. Phase 4
  subtracts `L` from the recording start position. Device round-trip latency (input + output) is
  not compensated today either; fix it in the same place.
- **Export.** Link is bypassed during a bounce. The bounce renders `totalSamples + L` and drops
  the first `L` samples so the file lines up with the timeline, and appends the longest
  `getTailSeconds()` when "include effect tails" is set (config flag, default on for project
  bounces).

Delay lines carry over between plans by track/aux id (direct-out delays by track id + channel in track). Hosted plugins that change latency at
runtime trigger a rebuild. Built in PR #257 with the compressor's look-ahead as the first such
parameter: `EffectSlot::hasStaleLatency()` makes the planner re-prepare the slot like a format
change, and `InsertChain::setParameter()` is the one write path that requests the rebuild.

### 2.5 Persistence, undo, agents

- JSON only ([`Streamable`](../../../Audionaut/Source/Engine/Streamable.h) writes JSON text; no
  binary format exists). Added keys are optional on read, so old projects load unchanged and
  `file_version` stays 1.
- **`AudioChannelData` stays untouched** (scalar, trivially copyable, `operator== = default`,
  consumed by the renderer, strips and tests as scalars only). Every read path assigns it
  wholesale from JSON (`channel->data = jsonElement; commitChannelData()`,
  [`AudioTrack.cpp:206`](../../../Audionaut/Source/Engine/Group/AudioTrack.cpp#L206),
  [`:144`](../../../Audionaut/Source/Engine/Group/AudioTrack.cpp#L144),
  [`UndoableChannelAction.h:92`](../../../Audionaut/Source/Engine/Undo/UndoableChannelAction.h#L92)),
  which would recreate any chain stored inside it. Instead `AudioChannel` gains a sibling member
  `SendList sends;` with its own `writeToJson`/`readFromJson` (it also had `InsertChain inserts`
  until PR #254).
  [`AudioTrack::writeChannelToJson`](../../../Audionaut/Source/Engine/Group/AudioTrack.cpp#L116)
  writes `channel->data` into the element and then a `"sends"` key next to it (an `"inserts"` key
  written by an earlier development build is ignored on read);
  `from_json` ignores unknown keys, so legacy and new files both load.
- **Instance-preserving `InsertChain::readFromJson`**: if sizes match and every
  `slots[i].type == j[i].type`, read in place (`setEnabled`, `effect->setState`), no rebuild.
  Otherwise reuse existing slots by type in order (so insert/remove in the middle keeps the
  others), create the rest through the registry, hand leftovers to the planner's retire path,
  then `requestRebuild()`.
- Track: `"inserts": [...]` next to `"channels"`. Container (`AudioTrackContainer::writeToJson`):
  `"aux_buses": [ {"id","name","gain","balance","mute","solo","inserts":[...]} ]` and
  `"master_inserts": [...]`. Living inside the container JSON gives aux buses whole-snapshot undo
  (`UndoableContainerAction`, `applyAsUndoableEdit(..., rebuild=false)`), agent batches
  (`AgentEditBatch` diffs the project JSON) and `documentOf` for free.

```json
"channels": [ { "gain": 1.0, "pan": 0.0, "mute": false,
                "sends":   [ { "aux": 1, "level": 0.5, "pre_fader": false, "mute": false } ] } ],
"inserts": [ { "type": "builtin:eq3", "enabled": true, "state": { "hp_hz": 80 } } ],
"aux_buses":      [ { "id": 1, "name": "Aux 1", "gain": 1.0, "balance": 0.0, "mute": false, "solo": false, "inserts": [ ] },
                    { "id": 2, "name": "Aux 2", "gain": 1.0, "balance": 0.0, "mute": false, "solo": false, "inserts": [ ] } ],
"master_inserts": [ ]
```

- Hosted plugin slot (phase 7): `{"type":"vst3:<uid>", "name", "manufacturer", "file",
  "state": "<base64>"}`; a missing plugin keeps its blob and shows as missing.
- Undo granularity: structural edits (add/remove/move/enable insert, add/remove aux, add/remove
  send) = whole-container snapshot with `rebuild=false`, the pattern routing changes already use;
  value edits (parameter drag, send level, aux return) = small `UndoableEffectParameterAction` /
  `UndoableSendAction` (addressed by trackId / channel / slot index, old/new value) with the
  `onDragStart`/`onDragEnd` gesture pattern, so playback is not disturbed. `UndoableChannelAction`
  stays scalar-only.
- Agent verbs are hosted automatically (every verb except `create` is hostable), so edits land
  as one undo step per task.

### 2.6 Known hazards to respect

- `AudioBusRenderer::process` builds `audioBusBlock`, `mainMixBlock` and `stereoBlock` over the
  *prepared* length
  ([`AudioBusRenderer.h:64`](../../../Audionaut/Source/Engine/Playback/AudioBusRenderer.h#L64),
  `:116`, `:128`); voices and gains advance the full length every block (latent bug). Phase 1
  takes `getSubBlock(0, numSamples)` for all three at the renderer entry.
- `AudioBusRenderer<double>` is instantiated but unused; effects are float only.
- `MAX_AUDIO_CHANNELS` is 64 in Debug, 128 in Release; plan vectors are sized by the live bus channel
  count.
- `applyMuteAndSoloGains` scans channel data only
  ([`AudioBusRenderer.h:213`](../../../Audionaut/Source/Engine/Playback/AudioBusRenderer.h#L213));
  aux solo is OR-ed in from `AuxState` in phase 3.
- [`Playback::processingBuffer`](../../../Audionaut/Source/Engine/Playback/Playback.h#L127) is
  one scratch buffer shared by all voices, and `stereoBuffer`/`mainMixBuffer`
  ([`AudioBusRenderer.h:290`](../../../Audionaut/Source/Engine/Playback/AudioBusRenderer.h#L290))
  are shared accumulators; phase 1 replaces them with per-worker scratch and per-track partials,
  which is what makes the track jobs independent (2.9).
- `AudioTrackContainer`'s constructor already takes 8 dependencies; the planner needs the
  container, so it is wired after construction with a setter (like
  `projectFileStore->setSerializer`).
- Effect CPU goes through the existing `DspLoadMeter`; the hidden bench (`[.][bench]`) gets an
  "N channels with EQ + compressor" case.

### 2.7 CLI, MCP, UI touch points

- CLI verbs follow `runTrackPan`
  ([`TrackCommands.cpp:102`](../../../Audionaut/Source/Cli/Commands/TrackCommands.cpp#L102)) and
  the `getCliCommands()` table in `CliDispatch.cpp`. `info` extends the per-channel object in
  `ProjectSummary.cpp` with `sends`, each track with `inserts`, and adds `masterInserts` and
  `auxBuses`.
- MCP: `Tools/audionaut-mcp/index.js` `registerTool` wrappers; `test.mjs` asserts the exact
  sorted tool-name list and the count word, update both.
- New `.cpp` files: `<FILE>` entries in `Audionaut/Audionaut.jucer`, then Projucer resave;
  reconfigure CMake after adding files.
- UI (phase 5): a mixer at the bottom of `MiddlePanelComponent` (`MixerComponent` under a
  `PanelResizerBar`), strips per channel grouped by track, aux and master strips, generic
  `EffectEditorComponent` from `EffectParameterInfo`; the arrangement's channel strips stay as
  they are. See phase 5.

### 2.8 What is taken from Tracktion, what is not

| Tracktion | Audionaut | Why |
|---|---|---|
| General node DAG rebuilt from the model on every structural change | Fixed mixer topology + a plain-data `MixPlan` rebuilt on structural change | Three summing points; a DAG compiler buys nothing and costs a scheduler, cycle detection and node state carry-over |
| Lock-free object swap, deferred destruction on the non-RT side | Same idea with the existing `LockFreeContainer` + `isPulled()` retire list | Already proven in `VoiceSourceContainer` |
| Sends and returns are inert plugins; a bus is an integer | Sends are channel properties; the aux bus is a first-class object with its own strip | Console mental model; a bus with a name, chain and return fader needs an object anyway |
| Volume/pan/meter are plugins; pre/post = chain position | Fader/pan stay fixed stages; sends carry a pre/post flag | Keeps the existing renderer, undo and UI; pre/post is the only ordering users need |
| PDC via latency nodes inserted at every summing node by a transform pass | Same rule, computed arithmetically for the three known summing points | No graph transform needed |
| Multithreaded node player: lock-free node queue, dependency counting, six wait strategies | Fork-join over track and aux jobs inside the callback (2.9): two barriers, the audio thread takes jobs too, semaphore wait between blocks | No DAG, so the jobs are known at plan build; ordered summation gives bit-identical output for any worker count |
| Automation streams under locks | Out of scope; parameters have stable ids and atomics, curves can come later | |

### 2.9 Multithreaded rendering: parallel across tracks, no added latency

**Answer first.** Parallelism costs no latency as long as it splits work that is independent
*within one block*: tracks are independent until the main mix, aux buses are independent of each
other, and all of that work happens inside the callback that outputs it. What costs latency is
pipelining *along* a chain (block n in effect A while block n−1 is in effect B, one block per
stage); some hosts' "multicore" modes work that way and this design does not. An insert chain is
always processed by one thread in order; the parallelism is across tracks (stage A) and across
aux buses (stage B). The hard limit is the critical path per block: the heaviest single track job
+ the heaviest aux job + the master chain must fit the block budget on their own. Everything else
divides by the number of workers.

**Why the track is the job.**
[`Playback::process`](../../../Audionaut/Source/Engine/Playback/Playback.h#L91) renders every
voice serially through one shared scratch buffer; a voice knows only its destination bus channel
(`VoiceSource::destinationChannel = dstChannel + track->getChannelOffset()`,
[`VoiceSource.cpp:147`](../../../Audionaut/Source/Engine/AudioSources/VoiceSource.cpp#L147)).
The time-stretch bench shows where the time goes: Rubber Band R3 costs about 8 % of a 128-sample
budget per track, flat across buffer sizes and bursty, so eight stretched tracks overrun one core
while the others idle. A track job that includes the track's voices gives the parallel renderer
its biggest win before the first effect exists; inserts are one more step inside the same job.

**Per-block schedule** (three stages, two barriers):

```
audio thread, serial:  invoke commander, pull plan, schedule pass (start/stop voices, PlayListScheduler::process),
                       input monitor + recorder pass (unchanged), bucket active voices by track
stage A, parallel over tracks (longest first):
   job(t) = voices of t into its bus channels → track inserts → PDC → gain + channel meter
            → pan into trackOut[t] (stereo) and send taps into sendOut[t][a]
── barrier ──
stage B, parallel over aux buses (skipped when no aux has inserts; the sums then fold into C):
   job(a) = Σ_t sendOut[t][a] in track order → aux inserts → return delay → return gain/balance/mute → auxOut[a]
── barrier ──
stage C, audio thread, serial:
   main = Σ_t trackOut[t] in track order → dry PDC delay → + Σ_a auxOut[a] → master inserts → master gain
   → device out; direct outs and stem copies from the post-gain channel buffers; master meter
```

**Track job in detail.**

- Voice bucketing: after the schedule pass the audio thread walks `voices[MAX_VOICES]` once,
  reads each active voice's bus channel (`VoiceSource::getDestinationChannel`) and counting-sorts the
  voice indices by the bus channel's track into a fixed `voiceOrder[MAX_VOICES]` with per-track
  begin/count. O(voices), no allocation; `Voice` and `VoiceSource` stay as they are. The job
  table itself needs no plan: bus channels of a track are contiguous and carry
  `AudioChannelData::trackId`, so runs of equal `trackId` over the renderer's channel data are the
  tracks (built per block, O(bus channels)); once the `MixPlan` exists it supplies the table with the
  chains attached.
- Each worker owns one mono scratch block: `processingBuffer` becomes `scratch[workerId]`. A
  voice's chain (`ClipTransportSource` → `StretchAudioSource` → `ClipResamplingSource` → reader)
  is per voice and only ever touched by the one job that owns its track, so its per-instance
  locks (`ClipResamplingSource::callbackLock`, `BufferingAudioSource`'s range lock) are never
  contended between jobs.
- Per-channel state (`gains[i]`, `panners[i]`, `channelLevel[i]`) and the track's inserts and PDC
  delay are owned by exactly one job. A job writes only its track's bus channels, its partial buffers and its own
  effects.
- Partial buffers replace today's shared `stereoBuffer`/`mainMixBuffer` adds: `trackOut`
  (tracks × 2 × maxBlock, sized to the channel maximum at prepare, 512 KB in Release) plus, from
  the sends phase, one stereo block per existing (track, aux) send pair allocated at plan
  build, so send memory follows what the project uses.
- **Determinism**: all sums run in track order on one thread, so the output is bit-identical for
  any worker count, including 0. A bounce with 0 workers and a live render with 7 produce the
  same samples; the tests rely on this.

**The pool.**

- `RenderThreadPool` (`Engine/Playback`), one per engine, owned by `AudioBusInterface` (no
  statics); `N` workers started in `prepareToPlay`, stopped in `releaseResources`. `N = 0` is
  the serial path as today, same code.
- Dispatch is a `Stage {jobs, count, fn}` with two atomics: `next` (`fetch_add` by every
  participant) and `remaining` (`fetch_sub` on completion). The audio thread publishes the
  stage, signals the workers, takes jobs from the same counter itself, then spins
  (`pause`/`yield`) until `remaining == 0`. With one job it simply runs it; with many it never
  idles while workers still run.
- Workers sleep on a counting semaphore between blocks and spin between stage A and B of the
  same block (a second wake would cost more than it saves). The wake-up latency (tens of µs)
  overlaps with the audio thread's own first job. If the bench at 32/64 samples shows the wake
  dominating, a "spin while playing" option is added; Tracktion ships six pool strategies for
  this reason, and this design starts with the one that burns no idle CPU.
- Jobs run longest-first (LPT): each job's cost is an exponential average of its measured ticks
  in a per-job runtime array; the audio thread insertion-sorts the job table by it every block
  (at most `MAX_AUDIO_CHANNELS` entries, allocation-free, a few µs), so a track that turns heavy
  (a clip switched to Stretch mode) moves to the front within a block, with no planner
  involvement.
- Memory model: a worker's writes happen-before the audio thread's `remaining.load(acquire)`
  through `fetch_sub(release)`; the stage publish is release/acquire the other way. Effect state
  is touched by whichever thread runs the job in a block, serialized by the barriers; for hosted
  plugins this is ordinary host behaviour (every DAW calls `processBlock` from worker threads).

**Priorities and the audio workgroup.**

- macOS: workers start with
  `juce::Thread::startRealtimeThread (RealtimeOptions().withApproximateAudioProcessingTime (blockSize, sampleRate).withPeriodMs (blockMs))`
  (time-constraint policy) and join the device's audio workgroup:
  [`LinkAudioDevice::audioDeviceAboutToStart`](../../../Audionaut/Source/Engine/Link/LinkAudioDevice.cpp#L83)
  hands `device->getWorkgroup()` to the pool, each worker holds a `WorkgroupToken` and re-joins
  when the workgroup changes (the documented pattern in JUCE's `juce_AudioWorkgroup.h`). The
  workgroup is what makes Apple silicon schedule the workers as part of the audio deadline;
  without it parallel audio on macOS is unreliable.
- Windows: JUCE's realtime thread is only `THREAD_PRIORITY_TIME_CRITICAL`; MMCSS "Pro Audio" is
  applied inside the WASAPI device thread alone, so each worker calls
  `AvSetMmThreadCharacteristicsW (L"Pro Audio")` itself.
- Linux: `SCHED_RR` through JUCE when `RLIMIT_RTPRIO` allows (the usual `@audio - rtprio 95`
  limits), otherwise normal priority, with a manual note.
- Default worker count: physical cores − 1 (`SystemStats::getNumPhysicalCpus()`, as the
  separation code does), capped at 8, leaving a core for the UI and the read-ahead thread.
  Settings → Audio gets "Processing threads: Auto / 1 … N" (1 = serial) in `Preferences`. The
  scratch engine and tests default to 0.
- **Offline rendering stays single-threaded** (decision 2026-10-26): `bounceProject`, CLI
  `export` and clip bounce run every stage serially on the calling thread, the `N = 0` path.
  An export has no deadline, the result is the same samples by construction, and the serial
  path is the reference the live tests compare against. The pool serves the device callback
  only; `AudioExporter` never starts it.

**Failure modes, stated.**

- A worker pre-empted mid-job makes the audio thread wait. With RT priority and the workgroup
  this is rare, and it is the exposure every multicore DAW has. The audio thread cannot abandon a
  job (the worker would write into channel buffers during the next block), so it waits; a wait
  longer than two block periods counts a `stall` next to the `DspLoadMeter` overrun counter,
  shown in the header meter tooltip.
- [`VoiceSource::waitForReadAhead`](../../../Audionaut/Source/Engine/AudioSources/VoiceSource.cpp#L74)
  blocks up to 2 ms live when the read-ahead thread is behind; existing per-voice behaviour, now
  inside a job. Parallel jobs overlap such waits instead of adding them, so per-block stall time
  can only drop. Priority inversion through `BufferingAudioSource`'s lock, held briefly by the
  normal-priority read-ahead thread, also exists today on the audio thread; not made worse,
  listed for completeness.
- Not parallelised: the schedule pass, stage C, and anything with a single job. One heavy track
  still has to fit the budget alone. (The per-channel input monitor and recorder calls move into
  the track job, see 2.10.)

**Render-ahead, a later option.** Because a track job is self-contained, a track with no
monitored or record-armed bus channel could be rendered k blocks early on the workers *between*
callbacks into a per-track FIFO, decoupling bursty jobs (Rubber Band) from the callback deadline
entirely. Timeline alignment is unchanged; what moves is control: parameter and fader changes
take effect k blocks later (k = 2 at 128 samples = 5 ms, inaudible), and seeks, stop, loop wraps
and tempo changes flush the FIFO. This is the "stretched render-ahead" item from the performance
notes and becomes "run track jobs early" on this infrastructure. It stays out of this plan's
phases: the fork-join renderer lands first and may make it unnecessary.

### 2.10 Track-to-track recording, prepared by phase 1

A bus channel of track B can take its input from track A instead of a device channel, and record or
monitor it. The per-track job already produces the signal (A's post-gain channel buffers and its
panned `trackOut[A]`); what is missing is a way to name a track as a bus channel input and a guarantee
that A's job has finished before B's job reads it.

- **Input source model.** Today a bus channel's input is one int, `inputChannel`, resolved against the
  device input block
  ([`AudioBusRenderer.h:76`](../../../Audionaut/Source/Engine/Playback/AudioBusRenderer.h#L76),
  [`effectiveInputChannel`](../../../Audionaut/Source/Engine/Channel/AudioChannelData.h#L42)).
  `AudioChannelData` gains a source kind: device channel, or track output (track id plus bus channel
  index for a mono channel-to-channel patch, or L/R of the track's stereo output). The struct stays
  scalar, so JSON (two optional keys), `commitChannelData`, `UndoableChannelAction` and the
  strip's input selector extend without new mechanisms. Removing a track resets bus channels that point
  at it, inside the same container snapshot.
- **Dependency ordering in stage A.** The per-block job table gets a level: tracks without track
  inputs are level 0, a track whose sources are all level 0 is level 1, and so on. Stage A runs
  as one fork-join sub-stage per level: one extra barrier per level, nothing when the feature is
  unused. Levels come from the channel data in O(tracks × depth) on the audio thread,
  allocation-free, with a depth cap. A cycle (A records B while B records A) leaves a track
  unassigned: its track inputs read silence and an atomic flag lets the UI show the conflict.
  Determinism and the serial path are unchanged, the sums still run in track order.
- **Phase 1 preparation.** The monitor add and the recorder call are per bus channel, read only the
  device input block and touch only that bus channel's recorder, so phase 1 moves them from the serial
  input pass into `renderTrack`, and the stage dispatcher takes a level count. A track-sourced
  bus channel is then a source switch: read from A's post-gain channel buffer or `trackOut[A]` instead of
  the input block. The recorder, its disk thread and the take-to-region step do not change.
- **Placement.** A device take is placed by the block's output time and, from phase 4, corrected
  for the device round trip. A track-sourced take must not get that correction, because the
  source was generated inside the same block for the block's timeline position; with PDC it is
  offset by the source track's pre-fader delay instead. The recorder subtracts a per-source
  latency, not a global one.
- **Double hearing.** If A still feeds the main mix, the user hears A and B's monitor of A at
  once. A gets an "internal only" output option next to main and direct out, so it plays only
  into tracks that record it. Mute would not do: mute silences the job's output and the recording
  with it.
- **Surface.** CLI `track-input --track B --channel 0 --from-track A [--from-channel 0 | --left |
  --right]`, MCP tool, `info`; a Tracks submenu in the strip's input menu and a routing
  indicator on the strip.
- **Later on the same mechanism.** Recording an aux bus output is a sub-stage after stage B.
  Recording the main mix is a bounce-in-place that needs B excluded from the mix it records;
  export covers that.

## 3. Implementation plan (one PR per phase, into develop)

### Phase 1: Parallel track rendering (`RenderThreadPool`)

Independent of effects: fixes the eight-stretched-tracks overrun on its own and leaves the
renderer in job form for the effect phases.

1. `Playback/RenderThreadPool.h/.cpp`: workers, counting semaphore, `Stage` dispatch with
   `next`/`remaining`, audio-thread participation + spin-wait, `startRealtimeThread` options,
   `WorkgroupToken` per worker, MMCSS call on Windows, stall counter.
2. `AudioBusRenderer`: sub-block fix; job table per block from runs of equal
   `AudioChannelData::trackId` over the bus channels, insertion-sorted by the per-job cost EMA;
   voice bucketing; per-worker scratch (`Playback::process` becomes
   `Playback::processVoice(index, scratch)`); `renderTrack(t, workerId)` (voices of the track,
   gain + meter, pan into `trackOut[t]`), `renderMaster()` (ordered sum, master gain, direct
   outs, stems); stages A and C from 2.9, stage B arrives with the aux buses; `trackOut` sized
   to the channel maximum at prepare. Preparation for 2.10: the per-channel monitor add and recorder
   call move into `renderTrack`, and the dispatcher runs stage A per level (one level today).
3. `LinkAudioDevice::audioDeviceAboutToStart`: workgroup to the pool;
   `PlayListScheduler::prepareToPlay` sizes the pool for the live device only; `bounceProject`
   and clip bounce stay serial; `DspLoadMeter`
   gains the stall count, header tooltip shows it.
4. `Preferences` key `processingThreads`, Settings → Audio control; CLI engines and the scratch
   engine never start a pool; tests default 0.
5. Jucer entries + resave; tests `ParallelRenderTests.cpp`: pool 0 / 1 / 3 bit-identical to
   each other and to `bounceProject` on a 16-track project with stretched and plain clips, pan,
   mute/solo and direct outs; add/remove track while playing; device restart restarts the pool;
   stall count stays 0. Bench case: 8 stretched tracks at 0 / 1 / 3 / 7 workers (the website
   performance page blocker).

### Phase 2: Effect interface, built-ins, inserts (channel, track, master), MixPlan

(Built with channel chains too; PR #254 removed them, see the phase 5 decision. The steps below
are kept as built.)

Built on `feature/effects-inserts` (2026-10-30). As built: the `MixPlan` lists `Channel`, `Track`
and `master` slots plus a `trackAtChannel` lookup; the renderer keeps its per-block job table
from the channel data and finds a job's track inserts by the job's first bus channel, skipping
them when the plan's channel count disagrees (the layout race). Slots carry the bypass
crossfade; the dry delay for latency waits for phase 4. A live slot whose format changes
(channel count, sample rate, a longer block) is replaced by a fresh instance with the same state
rather than re-prepared under the audio thread. The CLI verbs are `add-insert`, `remove-insert`,
`set-insert` and `list-effects`, the MCP tools `add_insert`, `remove_insert`, `set_insert` and
`list_effects`; `info` lists every chain.

Build order:

1. `Source/Engine/Effects/Effect.h`, `EffectTypeInfo.h`, `EffectParameterInfo.h` (header-only
   contract).
2. `Effects/EffectRegistry.h/.cpp` (no statics; injected).
3. `Effects/BuiltIn/TrimEffect`, `Eq3Effect`, `CompressorEffect` (`juce::dsp::Gain` /
   `IIR::Filter` / `Compressor`; atomics + `SmoothedValue`; deterministic `getState`).
4. `Effects/EffectSlot.h` and `InsertChain.h/.cpp` (JSON, instance-preserving `readFromJson`,
   add/remove/move/enable, retire hand-off).
5. `Effects/MixPlan.h`, `MixPlanContainer.h/.cpp` (`LockFreeContainer<MixPlan*>` + retire timer +
   `keepAlive`), `MixerPlanner.h/.cpp` (build from the container, `currentSpec`, prepare rules,
   sync rebuild in headless, `AsyncUpdater`).
6. `AudioChannel.h/.cpp`: `inserts` member + JSON helpers; `AudioTrack.h/.cpp`: `trackInserts`,
   write in `writeChannelToJson`, read in `readFromJson` and `mergeChannelFromJson`.
7. `AudioTrackContainer.h/.cpp`: `masterInserts`, `"master_inserts"`, planner setter,
   `requestRebuild()` in `publishNumAudioTrackChannels`.
8. `AudioBusRenderer.h/.cpp`: pull plan, clamping; the phase-1 `renderTrack` gains channel inserts
   and track inserts, `renderMaster` gains master inserts before `masterGain.process`; the job
   table and `channelToJob` come from the plan from now on (built at rebuild instead of per
   block), the per-block cost sort stays.
9. `PlayListScheduler.cpp`: `planner.prepare()` before the bus in `prepareToPlay`; flush pending
   rebuild in `bounceProject`.
10. `AudiumFactory.cpp`: registry, planner, plan container wiring.
11. `Engine/Undo/UndoableEffectParameterAction.h`; structure edits via
    `applyAsUndoableEdit(..., rebuild=false)`.
12. CLI `Cli/Commands/EffectCommands.cpp`: `add-insert --track --channel|--all-channels|
    --track-inserts|--master --type [--at]`, `remove-insert`, `set-insert --param --value |
    --enabled`, `list-effects`; dispatch table; `ProjectSummary`; MCP tools + `test.mjs` list.
13. Jucer entries + resave; tests `Catch2Tests/Tests/EffectTests.cpp` (registry creates every
    built-in for 1 and 2 channels; state round trip deterministic), `InsertRenderTests.cpp`
    (Trim −6 dB on one bus channel halves that bus channel in `LiveBlockDriver` render and in `bounceProject`;
    track insert sees both bus channels; master insert affects main only, not direct outs; add/remove
    insert while playing stays click-free, `compareBuffers` vs bounce; undo/redo restores JSON and
    keeps effect instances; save/reopen; legacy project loads with empty chains; headless
    add-insert then export applies the insert; CLI envelope test; pool 0 / 3 bit-identical with
    inserts). Bench case: 64 channels EQ + compressor at 0 / 1 / 3 / 7 workers.

### Phase 3: Sends and aux buses

Built on `feature/effects-sends` (2026-10-31, off `feature/effects-inserts`). As built: `AuxBus`
holds a `shared_ptr<AuxState>` (gain, balance, mute, solo atomics plus the return meter) and an
`InsertChain`; `AuxBusContainer` is a member of `AudioTrackContainer`, starts with the two
defaults at construction and on `cleanup()`, reads `"aux_buses"` in place by id (absent key =
defaults, empty array = none), and never reuses an id. `SendList` is a sibling of `inserts` on
`AudioChannel`, one `SendEntry {auxId, shared_ptr<SendState>}` per aux at most, kept in aux id
order, `"sends"` written only when there is one. The plan gains `Send {aux, pair, state, gain}` per
channel, `Aux {id, state, pairs, inserts, gainLeft, gainRight}` in strip order, and owns the
`sendOutput` (one stereo pair per send) and `auxOutput` (one per aux) buffers, sized at build;
the planner seeds every `juce::dsp::Gain` at its target before `prepare`, so a new plan never
ramps. Send levels and aux return gains are linear like the channel gain; the return balance
attenuates the side it turns away from (`left = gain · min(1, 1 − b)`, `right = gain · min(1, 1 + b)`)
and does not move signal across. Aux solo: with any aux soloed the dry main mix (a `dryGain` on the
renderer) and the non-soloed returns go silent; sends follow channel mute/solo unless pre-fader.
Pre-fader taps use the panner's balanced rule statelessly from `AudioChannelData::pan`; post-fader
taps reuse the panned block the main mix gets. `removeAuxBus` on the container drops every send
into the bus (undo brings both back through the snapshot). Stage B runs through the pool only when
there is more than one aux. A one-channel output (mono device, mono bounce, `monoMain`) has no
pan: its sends tap the bus channel as it is on both sides, the return gain ignores balance, and the
return's first side joins the mono mix; a stem export carries no sends or returns (its outputs are
the dry bus channels). Merged with develop after PR #246 (the inserts review round) on 2026-10-31. CLI: `add-aux [--name]`, `remove-aux --aux`,
`set-aux --aux [--name] [--gain] [--balance] [--mute] [--solo]`, `set-send --track [--channel]
--aux [--level] [--pre-fader] [--mute]` (every channel of the track without `--channel`; the send
is made on first set), `remove-send`; the insert verbs take `--aux ID` as a chain; `info` lists
`auxBuses` and per-channel `sends`. MCP: `add_aux`, `remove_aux`, `set_aux`, `set_send`,
`remove_send`, `aux` on the chain params (32 tools). Undo: `UndoableSendAction`,
`UndoableAuxAction` for values, `InsertChainAddress::ofAux` for aux inserts.

1. `Effects/AuxBus.h/.cpp`, `AuxBusContainer.h/.cpp` (stable ids, `AuxState` atomics, insert
   chain, `createDefaults()` = "Aux 1" and "Aux 2"), `SendEntry.h` (`SendState` atomics),
   `SendList`.
2. `AudioChannel`: `sends` member + JSON; `AudioTrackContainer` owns `AuxBusContainer`,
   `"aux_buses"` JSON (written always, read in place by id; a missing key creates the two
   defaults, an empty array means the user removed them), defaults created in `cleanup()` so a
   new project starts with them, rebuild on add/remove.
3. `MixPlan`/`MixerPlanner`: `sendOut` partials per (track, aux) pair sized at build, per-channel
   sends, aux chains, aux solo folded into the solo rule.
4. `AudioBusRenderer.h`: post-fader taps from the bus channel's panned stereo in `renderTrack`,
   pre-fader taps with the balanced gain pair before `gains[i].process`, `renderAux(a, workerId)`
   as the stage B job (ordered sum of `sendOut[·][a]` → inserts → return gain/balance/mute →
   meter), aux returns summed into the main mix in stage C before master inserts.
5. Undo (`applyAsUndoableEdit` for structure, `UndoableSendAction` for levels), CLI
   `add-aux`/`remove-aux`/`set-aux`/`set-send --track --channel --aux --level [--pre-fader]
   [--mute]`, MCP, `ProjectSummary`, jucer, tests `SendRenderTests.cpp` (new project has Aux 1
   and Aux 2 and renders silence to both; legacy project without `aux_buses` opens with the two
   defaults, saved JSON lists them; a project saved with an empty `aux_buses` array stays empty;
   level −inf → aux silent; pre vs post fader with channel gain 0; aux insert Trim; mute/solo
   matrix incl. aux solo; JSON round trip; undo of remove-aux brings the bus back with its sends).

### Phase 4: Latency compensation and export alignment

Built on `feature/effects-pdc` (2026-10-31, off `feature/effects-sends`). As built: `PdcDelay` is a
fixed whole-sample delay for N channels (a ring of delay + block length, written then read, so any
block length up to the prepared one works). `EffectSlot` reads `getLatencySamples()` once at
`prepare()`, and a latent slot owns a `bypassDelay` of that length for its dry signal: bypassed it is
as late as running, the crossfade mixes aligned signals, and on the return from a bypass the delayed
dry carries on alone for the effect's latency (its first output is the pre-roll of a signal it has
not heard yet), then the crossfade starts aligned - so a latent effect comes back its latency later
than switched on, and the block that switches it does no extra work (review of PR #250: replaying
the dry line's history into the effect was a burst of ceil(latency / block) effect calls in one
callback). `MixPlan::Latency
{preFader, aux, master}` carries the arithmetic of 2.4 (`atMainOutput()`, `atStemOutput()`), the
plan owns `mainDryDelay`, per-track `preFaderDelay` (per-channel until PR #254 removed the channel
chains), per-channel `directOutDelay`, per-aux `returnDelay`,
and `tailSeconds(stemExport)` (Effect::getTailSeconds of the enabled slots summed along a path, the longest path at each point, up to the fader point for stems; read from the effects on demand so a bounce takes the current delay time, not the one at plan build - value changes rebuild no plan).
`MixerPlanner::planLatency` computes them after the chains are collected; `delayFor()` carries a
delay over from the previous plan by (kind, track id, channel in track | aux id; a track's pre-fader
delay since PR #254 by track id alone) when the wait is
unchanged, so a rebuild while playing leaves the signal in flight intact (tested). The renderer runs
the delays on the stage that owns the signal: the fader point after the track inserts (stage A),
the return delay after the aux inserts (stage B), the dry delay before the returns join and the
direct-out delay on the way to the output (stage C, a delay reset when its channel was not routed
to a direct out the block before); stems leave at the fader point. `AudioBusRenderer::
getOutputLatencySamples()` reads the adopted plan (`atOutput(stemExport)`) and `AudioBusInterface`
forwards it, together with the device's input/output latency from `audioDeviceAboutToStart`.
`LinkAudioDevice::latencyToOutput()` = device output latency + compensation feeds both the
`audioCallback` stamp and `beatAtTime`; the test harness uses the same method. For a solo session
the term cancels (the start is pinned in the same compensated time), so the live tests check the
pure shift and the figure itself; only peers see the difference. `PlayListScheduler::startRecording`
places the take `getRecordingLatencySamples()` (device round trip + compensation) before the stamped
position, clamped at the timeline's start. `bounceProject` renders `L` samples more and drops the
first `L` (the stems' `L` for a stem export), appends the tail for a bounce to the project's end
unless `ExportAudioConfig::includeEffectTails` is off (CLI `export --no-tails`, MCP `include_tails`),
and resets the chains when it ends as well as when it starts, so live playback never hears its
tail. `LatencyCompensationTests` ([latency]): `PdcDelay` with mixed block lengths; a 480-sample
insert shifts live outputs (main and direct outs) exactly and the bounce equals the plain
one sample for sample, for 0 and 3 workers, stems and mono too; track chains of different latency
align (since PR #254; channel + track chains added up before); bypassed
and toggled while playing; aux and master points alone and all three together (1036); a rebuild
while playing carries the delay lines over; Link term and recording placement with a fake device of
64/128 samples; the tail extension.

- `getLatencySamples()` honoured; `PdcDelay` (fixed integer delay FIFO) per track/aux/main/direct out;
  arithmetic from 2.4 in `MixerPlanner::build`; delay state carried over by id; bypass delay in
  the slot.
- Planner publishes `totalLatency` (atomic samples). `LinkAudioDevice` adds it to the output-time
  term that feeds `beatAtTime` and `LinkEngine::audioCallback`; `Recording` subtracts it (plus
  device round-trip latency) from the take's start position; `bounceProject` pre-rolls by `L` and
  drops `L` samples, optional tail extension.
- Test-only `LatencyTestEffect(N)` registry entry: a click on channel A with latency 512 and channel B
  without lands on the same sample in the bounce; aux chain latency keeps dry/wet aligned; bypass
  toggle keeps alignment; with the fake device and a 480-sample insert, a click scheduled at beat
  1 leaves the output at the same device sample as without the insert (Link compensation); a
  recorded click lands at the position it was played.

### Phase 5: Mixer view

Decided with Klaus 2026-11-06, replacing the earlier idea of effects rows inside the arrangement's
channel strips: effects get a **mixer at the bottom of the middle panel**, under the channel column
and the arrangement, with the right panel untouched. The arrangement's channel view stays the
compact strip it is. Whether the mixer is open and how tall it is are app preferences
(`MixerVisible`, `MixerHeight`), default closed; View ▸ Mixer (Cmd+Shift+M) and a header button
next to the right-panel button toggle it.

- **Frame (5a, first step, built 2026-11-06):** `PanelResizerBar` (shared with the right panel
  seam, which it replaces: one pixel seam line, wider grab zone, highlighted under the mouse,
  double-click resets the size) above a `MixerComponent` in `MiddlePanelComponent`. Height 180
  min, 300 default, half the panel max. Open state, size and the open/close animation live in
  `CollapsiblePanel`, one per panel: like Xcode's debug area, dragging the seam past the minimum
  collapses the panel while the drag goes on, dragging back brings it back, the release decides,
  and a panel closed by a drag keeps its previous size for reopening. The right panel got the
  same treatment (explicit, persisted width `RightPanelWidth` and `RightPanelVisible` instead of
  the proportional `StretchableLayoutManager` split).
- **Strips (5a, built 2026-11-06):** as designed below, in `Interface/Components/MixerPanel/`:
  `MixerComponent` (viewport of track groups; a structure signature decides rebuild vs. refresh;
  owns the editor windows), `MixerTrackGroupComponent` (band + strips), `MixerStripComponent`,
  `MixerStripElements` (StripSends / StripIo / StripPan / StripButtons / StripFader / StripName, one
  component per strip element), `MixerLayout.h`, `MixerSectionBar.h`, `InsertSlotListComponent`, `SendRowComponent`, `EffectEditorComponent`, and `MixerContext.h`
  with the undoable chain edits and menus (`InsertChainActions`, address-resolved at callback
  time). Lessons: app-shell calls in interface code stay behind `#ifndef AUDIONAUT_HEADLESS` (the
  tests and the CLI compile every interface source); the application's `getNextCommandTarget()`
  now hands commands on to the main window so shortcuts work from tool windows.
  Design: one 76 px strip per channel grouped per track under a coloured track band
  (name only; the arrangement's minimize does not apply to the mixer). Strip top to bottom: name, insert slots
  (click = editor window, right-click = Bypass / Remove / Replace ▸ / Move, drag reorders, "+" adds
  from the registry), one send row per aux (mini level bar, a send that does not exist yet is dim
  and created by the first drag, right-click = Pre-Fader / Mute / Remove), pan, R I S M, vertical
  fader with level meter, dB readout, routing. Fixed heights (2026-11-08, `MixerLayout.h`): every
  element has one height, the mixer's height follows from the shown sections and the send rows
  (`MixerLayout::mixerHeight`), the seam above it only closes and reopens it (`CollapsiblePanel::
  setFixedSize`), the `MixerHeight` preference is gone; no height tiers any more. Values and meters refresh on the 60 Hz timer; structure follows
  `updateChannelsAction` / `rebuildAll`. Undo: structural changes through `applyAsUndoableEdit`,
  values through `UndoableEffectParameterAction` / `UndoableSendAction` on drag end.
- **Decision (2026-11-07): no per-channel inserts.** Insert effects belong to the track and run at
  the track's channel count; the mixer shows one insert list per track (across the group, under
  the band, `InsertChainAddress::ofTrack`), the channel strips carry sends, I/O, pan, buttons and
  fader only. The engine followed in PR #254 (merged 2026-11-11): `AudioChannel::inserts`,
  `InsertChainAddress::ofChannel`, the planner's channel stage and the CLI/MCP channel addressing
  are gone, the pre-fader PDC delay is one n-channel delay per track, and `add-insert` /
  `remove-insert` / `set-insert` reject `--channel` / `--all-channels` with a usage error.
  Editor windows are titled "Track 2 / EQ3".
- **Section bar (2026-11-08):** a 30 px bar at the mixer's left (`MixerSectionBar`) with five
  icon toggles that show or hide strip sections in every strip: inserts, sends, I/O, pan, the
  R I S M buttons (fader, meter and name always show). The mask is the app preference
  `MixerSections` (flags of `MixerLayout::Section`); hidden sections give their height to the fader.
- **Editor (5a):** generic `EffectEditorComponent` from `EffectParameterInfo` (one row per
  parameter: label, slider with unit, toggle for booleans; bypass in the header) in a non-modal
  `FloatingToolWindow` per slot, titled "Track 2 / ch 1 / EQ3", closing itself when its slot,
  channel or project goes away.
- **Aux and master (5b):** after a divider one strip per aux bus (editable name, inserts, pan, S M,
  fader, stereo meter, remove via the name's menu) and a "+" column that adds one; the master strip
  pinned right of the scrolling area (inserts, the master fader of the header, stereo meter).
- Open: a visible output latency figure (header) and an `info` latency field, carried over from
  phase 4.

### Phase 6: Remaining built-ins (dropped 2026-11-13)

- Was: Limiter, Gate, Reverb, Delay, Chorus, Phaser. Dropped with the decision in section 1: the
  built-ins stay development tools, so phase 7 comes next.

### Phase 7: Plugin hosting (AU on macOS; VST3 on Windows; VST3 and LV2 on Linux)

Decided with Klaus 2026-11-14: **macOS stays sandboxed on both channels** (App Store and DMG
share one container), so the Mac hosts Audio Units only - AUv2 in-process when sandbox-safe,
otherwise (and every AUv3) out of process, see 7c; Windows hosts VST3, Linux VST3 and LV2. The
three `JUCE_PLUGINHOST_*` flags are set globally (jucer `JUCEOPTIONS`, test CMake), the formats a
process hosts are chosen at runtime (`PluginHost::addPlatformFormats`). Cross-format recall:
the slot JSON carries a `"plugin"` block (name, manufacturer, format, file, version) beside the
type id `au:`/`vst3:`/`lv2:` + eight hex digits of the plugin's unique id; a project saved with
the plugin in one format opens with it in another installed format by name + manufacturer, the
state blob handed over. Scanning runs this executable as a child process per plugin file
(`scan-plugin` verb). Two PRs: 7a engine, 7b UI.

Built in 7a (`feature/plugin-hosting`, 2026-11-14): `Engine/Effects/Hosting/` - `PluginHost`
(one per process, injected into the factory: format manager, `KnownPluginList` as
`<data>/Plugins/KnownPlugins.xml` with the blacklist and a dead man's pedal, `attach(registry,
mode)` registering every listed plugin as an effect type by manufacturer and again on every list
change, the substitute resolver, `scanAll`, `useChildProcessScanner`; `Mode::catalogueOnly`
gives the scratch engine of a hosted edit placeholders that carry the state instead of a
second instance of every plugin), `HostedEffect` (parameters normalised 0..1 under the plugin's
ids, the plugin's text; state `{"blob": base64}` re-read only after the plugin reported a
change, equal blobs skipped on set; layout negotiation with a mono-chain fold through
stereo-only plugins, refused layouts pass dry as `incompatibleLayout`; a latency change the
plugin reports posts `Effect::onLatencyChanged`, which the chain turns into a plan request),
`ChildProcessScanner`, `PluginTypeId`. `Effect`'s parameter methods became virtual;
`EffectRegistry` gained `remove` and the substitute resolver, `MixerPlanner::createSlot` the
element it may substitute for, `InsertChain` the `"plugin"` block, the latency hook and
`getUnresolved`/`removeUnresolved`. CLI: `list-effects` names plugins without loading them,
`--type` loads one; `scan-plugins`, `scan-plugin` (worker); `info` lists hosted inserts with
their plugin block and missing elements with `"missing": true`. MCP: `list_effects` takes
`type`, texts updated, no new tool. Tests: `TestPluginInstance` (an in-process
`AudioPluginInstance` with hosted parameters), `HostedEffectTests`, `PluginHostTests`, and on
macOS Apple's AUDelay through the real format (`AudioUnitHostingTests`, ctest `CliScanAU`).

Built in 7b (`feature/plugin-hosting-ui`, 2026-11-14): `SlotEditor`, the interface the mixer
asks an editor window's content through (`isStillValid`, `onSlotGone`), implemented by the
built-in `EffectEditorComponent` and the new `HostedEffectEditorComponent` (the plugin's own
editor from `createEditorAndMakeActive` and nothing around it, JUCE's generic parameter list for
a plugin without one, name and reason as text for a placeholder; the slot held strongly so the
instance outlives its editor; sized to the editor and following it). Klaus's call: the editor
windows carry no band of their own - the built-in editor lost its bypass key too (bypass is the
slot's menu; a bypassed slot's editor is dimmed) and the compressor's gain-reduction meter moved
to a strip under the dials - and an insert added from the mixer opens its editor at once
(`InsertChainActions::addAndEdit`). `FloatingPanel` follows its content's size (a plugin that resizes its editor resizes the
window) and, for an editor the plugin lets the user resize, is resizable itself within the
editor's limits (macOS: `NSWindowStyleMaskResizable`, `windowDidResize:`, content min/max size;
elsewhere `DocumentWindow::setResizable`). The Manage Plugins window (`PluginManagerComponent`:
`juce::PluginListComponent` over the host's format manager, list and dead man's pedal, one
scanning thread so the worker processes run one at a time, a status line counting effects,
instruments and skipped files) opens from *Options › Manage Plugins…*
(`CommandIDs::showPluginManager`) and from the end of every insert type menu
(`MixerContext::openPluginManager`). `InsertSlotListComponent` shows a chain's unresolved
elements as dimmed rows with a marker at their saved places (tooltip "<name> by <manufacturer>
(<format>) is not installed", right-click Remove → `InsertChainActions::removeUnresolved`, one
undo step), and marks a hosted slot that is not running (not loaded, incompatible layout) the
same way with the reason as tooltip; a drop among the rows maps back to slot indices. Manual
chapter 14 (mixer, inserts, sends, solo groups, plugins, Manage Plugins, missing plugins, the
CLI side), `features.md` moved hosting out of *Not yet*.

**Built in 7c (out-of-process Audio Units).** 112dB's Mikron Delay would not load in the app: inside
the sandbox the OS refuses `AudioComponentInstanceNew` with -3000 for any AUv2 without
`kAudioComponentFlag_SandboxSafe` (its `resourceUsage` wants `files.all.read-write`, which no App
Store entitlement grants; 17 of the Audio Units on Klaus's Mac are like that). What works, verified
with a sandboxed test tool, is what Logic does: `AudioComponentInstantiate` with
`kAudioComponentInstantiation_LoadOutOfProcess` puts the plugin into Apple's `AUHostingServiceXPC`,
and `requestViewController` delivers its editor as a remote view. JUCE took that path for AUv3 only,
so the JUCE submodule now points at Klaus's fork (`kvoltmer/JUCE`, branch
`audionaut/au-out-of-process`, to be offered upstream): `AudioUnitPluginFormat::setProcessIsolation`
(`inProcess` | `whenSandboxRequires` | `outOfProcess`) decides per component, such plugins report
`requiresUnblockedMessageThreadDuringCreation`, and the editor window skips the in-process Cocoa view
for them. `PluginHost::addPlatformFormats` sets `whenSandboxRequires`. Creation is asynchronous:
`PluginHost::createEffect` hands out a `HostedEffect` in status `loading` (passes through, keeps the
blob), `createPluginInstanceAsync` delivers the plugin on the message thread, `adopt()` takes it,
applies the kept state and sets `needsPrepare()` (new on `Effect`, part of `EffectSlot::isPreparedFor`)
so the planner prepares the slot before the next plan runs it; `onLatencyChanged` asks for that plan.
`PluginHost::waitForPendingLoads` pumps until the plugins arrived - the CLI calls it after opening a
project, after `add-insert`, in `list-effects --type` and before a save (`ProjectSession::settlePlugins`).
`Availability` is down to `available`/`formatMissing`; `loadsOutOfProcess` tells the Manage Plugins
status line how many run in a process of their own; the editor window of a loading slot shows
"Loading the plugin..." and takes the editor when it comes. AUv3 came along for free.

Open after 7c: the editor windows saved with the project (TODO.md, mixer follow-ups); measuring the
cross-process render cost; a window opened on a loading placeholder stays non-resizable until reopened.

- Jucer: `JUCE_PLUGINHOST_VST3=1`, `JUCE_PLUGINHOST_AU=1` (macOS, `AudioUnit` framework),
  `JUCE_PLUGINHOST_LV2=1` (Linux). Sandbox allows AU only (in-process when sandbox-safe, out
  of process otherwise, see 7c) - on both macOS channels, see above.
- `PluginRegistry` service (per process, injected, not static): `juce::AudioPluginFormatManager`
  + `KnownPluginList` persisted as XML in `Preferences::applicationDataDirectory()`; scanning in
  a child process (JUCE `ChildProcessCoordinator`, dead-man list so a crashing plugin is skipped
  on rescan).
- `HostedEffect : Effect` wrapping `juce::AudioPluginInstance`: bus layout negotiation to n
  channels, `processBlock` with an empty `MidiBuffer`, `getLatencySamples` → planner rebuild on
  change, `getStateInformation` base64 in the slot JSON, missing-plugin placeholder.
- Editor window (`createEditorIfNeeded`) in Interface; settings page for scan paths and rescan;
  CLI `scan-plugins`; hosted plugins process headless.
- The runtime latency rebuild exists (PR #257): a hosted plugin's latency change only has to
  mark its slot stale.
- Release builds: the mixer's insert slots and menu list hosted plugins (empty until then).
- Manual chapter on effects; `features.md` moves the item out of *Not yet*.

### Phase 8: Track-to-track recording

Can land any time after phase 1.

1. `AudioChannelData`: `inputSource` kind + `inputTrackId` + `inputChannelIndex` scalars, optional JSON
   keys, `effectiveInput()` replaces `effectiveInputChannel`; `AudioTrackContainer::removeTrack`
   resets dependents in the snapshot.
2. `AudioBusRenderer`: levels in the job table, stage A per level, source switch in
   `renderTrack` (device block, A's post-gain bus channel, `trackOut[A]` L/R), cycle flag; "internal
   only" output value for `outputChannel`.
3. `Recording`: per-source placement latency (device round trip vs. source pre-fader delay).
4. CLI `track-input`, MCP tool, `info`, undo via `UndoableChannelAction`; strip input menu
   Tracks ▸, routing indicator, cycle warning.
5. Tests `TrackInputTests.cpp`: recording A's clip onto B yields A's samples at the same timeline
   position (pool 0 and 3); A → B → C chain; cycle detected and silent; after phase 2, B's take
   includes A's inserts; "internal only" keeps A out of the main mix; undo restores routing.

## 4. Verification (every phase)

```
cmake -B build -S Audionaut/Catch2Tests && cmake --build build -j8
./build/AudionautTests_artefacts/AudionautTests "[effects]"
./build/AudionautTests_artefacts/AudionautTests "[.][bench]"      # CPU before/after on the 64-channel project
```

Then the app build (jucer resave, Xcode Debug): play the bench project with inserts while adding
and removing effects (no clicks, DSP meter stable), undo/redo insert edits during playback. Hosted
agent path: an `add_insert` MCP call on the open document shows up as one undo step. Headless:
`audionaut-cli add-insert ... && audionaut-cli export ...` renders with the insert.
