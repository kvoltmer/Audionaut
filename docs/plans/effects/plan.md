# Insert and send effects: signal design and implementation plan

## 1. Context

Audionaut has no effects. [`features.md`](../../features.md) lists "Plugin hosting (VST3 / AU /
LV2), effects and automation envelopes" under *Not yet*.

The mixer is one flat array of mono bus lanes (`MAX_AUDIO_CHANNELS`,
[`PlaybackDefines.h`](../../../Audionaut/Source/Engine/Playback/PlaybackDefines.h)). Every
`AudioChannel` of every `AudioTrack` owns one lane at `track.getChannelOffset() + channelIndex`
([`AudioChannel.cpp:114`](../../../Audionaut/Source/Engine/Channel/AudioChannel.cpp#L114)), voices
sum into their lane ([`Playback.h:95`](../../../Audionaut/Source/Engine/Playback/Playback.h#L95)),
and [`AudioBusRenderer::process`](../../../Audionaut/Source/Engine/Playback/AudioBusRenderer.h#L54)
applies gain (mute/solo folded in), pan into a stereo main mix or a direct device output, then
master gain. All mixer state lives in the renderer and is written only through `LockFreeCommander`
lambdas.

This plan adds **insert effects** (per lane, console style, plus an optional per-track block that
sees all lanes of a track at once) and **send effects** (per-lane aux sends into dedicated FX buses
that return to the main mix), in phases.

Decisions taken (2026-10-25):

- Built-in effects first (juce_dsp, already a project module, no new dependencies). VST3/AU
  hosting implements the same interface in a later phase.
- Inserts per lane, like an analog console; optionally a per-track insert block processing all
  lanes of a track together (stereo effects on a stereo track).
- Send destinations are dedicated FX buses shown as strips in the channel view, not tracks. The
  master bus gets an insert chain too.

Tracktion Engine (GPL3/commercial) was studied for ideas only. Nothing is ported; section 2.8
lists where this design differs and why.

## 2. Signal design

### 2.1 Topology (fixed, not a general graph)

```
hardware input (monitor) ─┐
voices (clips) ───────────┴─► lane k ─► [lane inserts k] ─┐
                                                          │ for each track t: lanes [off..off+n)
                                           [track inserts t] (n-channel block, in place)
                                                          │
                                                   PDC delay (pre-fader align)
                                                          │
                                 ┌── pre-fader send taps ──┤
                                 │                     lane gain (mute/solo) ─► lane meter
                                 │                        │
                                 │                  pan (mono→stereo)
                                 │                        │
                                 └─► post-fader send taps ┤
                                                          ├─► main mix (stereo)      or   direct out (post-gain, unpanned)
aux bus a (stereo): Σ sends ─► [aux inserts a] ─► PDC ─► return gain/balance/mute ─► + main mix
main mix ─► PDC (dry delay) ─► + aux returns ─► [master inserts] ─► master gain ─► master meter ─► device out 0/1
```

- **Lane insert**: mono chain on one bus lane, pre-fader. The console-strip model.
- **Track insert**: n-channel chain over the contiguous lanes of one track (n = track channel
  count), after the lane inserts, before the fader. Contiguity holds because
  `commitChannelData()` places lanes at `channelOffset + index`. Processed with
  `AudioBlock::getSubsetChannelBlock(offset, n)` in place, no copy.
- **Sends** are per lane: each lane has 0..N entries `{auxId, level, preFader, mute}`. A send is
  *post-pan*: the lane's mono signal is panned with the lane's pan into the stereo aux. Post-fader
  sends tap the lane's `stereoBlock` right after `panners[i].process`
  ([`AudioBusRenderer.h:131`](../../../Audionaut/Source/Engine/Playback/AudioBusRenderer.h#L131)),
  exactly what the main mix gets, scaled. Pre-fader sends tap before gain with a stateless gain
  pair from the same balanced pan rule (`juce::dsp::Panner` default, `Rule::balanced`), so they
  diverge from the main pan only during a pan move. Sends follow mute/solo unless pre-fader.
- **Aux bus**: stereo, first-class object (`AuxBus`): stable id, name, insert chain, return gain,
  balance, mute, solo. v1 routes only to the main mix. No aux→aux sends in v1, so the topology is
  acyclic by construction and needs no cycle detection. Aux→aux with a topological order is a
  later extension.
- **Master inserts** sit before master gain, so the master fader stays the last stage.
- **Direct outs** (`outputChannel >= 0`) keep today's semantics (post-gain, unpanned, no master),
  get PDC alignment (2.4), and can still feed sends.
- **Stem export** (`config->numChannels > 2`,
  [`PlayListScheduler.cpp:756`](../../../Audionaut/Source/Engine/PlayList/PlayListScheduler.cpp#L756))
  copies lanes post-inserts, post-gain as today; aux returns are not in stems (standard DAW
  behaviour, to be documented).
- **Monitoring** input is added to the lane before the inserts, so monitored input is heard through
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

- Channel support: `EffectTypeInfo::supportsChannels(int n)`. Lane inserts need mono; track, aux
  and master inserts need n-channel. Built-ins support 1..n generically.
- `EffectSlot` (`shared_ptr`-owned): `{type, enabled, shared_ptr<Effect>, bypass crossfade +
  bypass delay runtime}`. `InsertChain` = vector of `shared_ptr<EffectSlot>` + JSON in/out +
  add/remove/move/enable. One `InsertChain` type serves lane, track, aux and master.
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
    struct Lane  { std::vector<EffectSlot*> inserts; std::vector<Send> sends; int preFaderDelay; };
    struct Track { int firstLane, numLanes; std::vector<EffectSlot*> inserts; };
    struct Aux   { AuxState* state; std::vector<EffectSlot*> inserts; int returnDelay; juce::dsp::Gain<float> gain; };
    std::vector<Lane> lanes; std::vector<Track> tracks; std::vector<Aux> auxes; std::vector<EffectSlot*> master;
    int mainDryDelay, totalLatency; double sampleRate; int maxBlockSize;   // the spec it was prepared for
    std::vector<std::shared_ptr<EffectSlot>> keepAlive;   // every slot the plan references: survives model rebuilds
    std::vector<std::unique_ptr<PdcDelay>> laneDelays, auxDelays; PdcDelay mainDelay, directOutDelay; // carried over by id
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
  gain/balance/mute/solo (`AuxState` atomics), lane gain/pan/mute/solo (unchanged commander path).
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
  the published lane count and lane data are already up to one block apart. The plan is built
  from the same track vector; the renderer clamps: a `Track` with
  `firstLane + numLanes > audioBusChannels` is skipped, lanes iterate to
  `min(plan.lanes.size(), audioBusChannels)`.
- **Audio thread invariants**: no allocation (aux buffers and delays sized at build), no locks,
  plan pointer read once per block, loops bounded by the plan's vectors and `MAX_AUDIO_CHANNELS`.
  New commander lambdas capture pointers/ints only (the existing `AudioChannelData` capture
  already exceeds libc++'s 24-byte small buffer and heap-allocates; do not make it worse).
- Two engines per process (scratch engine, tests) each own a planner, registry and plans; no
  static state.

### 2.4 Latency compensation (PDC), designed in, implemented in phase 3

Rule: *latency adds along a path; at every summing point each input is delayed to the longest*.
The topology has exactly three summing points, so the delays are computed arithmetically in
`MixerPlanner::build`, no graph search:

| Point | Inputs aligned | Delay inserted |
|---|---|---|
| Fader (per lane) | lane inserts + its track inserts | `preFaderDelay[k] = Lpre − (laneLat[k] + trackLat[t(k)])`, `Lpre = max over lanes` |
| Main mix + aux returns | dry main vs. each aux chain | `mainDryDelay = Laux = max auxLat[a]`, `returnDelay[a] = Laux − auxLat[a]` |
| Device output | main path vs. direct outs | direct outs delayed by `Laux + Lmaster` |

Total output latency `Lpre + Laux + Lmaster` is reported to the scheduler (playhead display offset)
and to the export: a bounce renders `totalSamples + L` and drops the first `L` samples so the file
lines up with the timeline, and appends the longest `getTailSeconds()` when "include effect tails"
is set (config flag, default on for project bounces). Delay lines carry over between plans by
lane/aux id. Hosted plugins that change latency at runtime trigger a rebuild.

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
  which would recreate any chain stored inside it. Instead `AudioChannel` gains sibling members
  `InsertChain inserts; SendList sends;` with their own `writeToJson`/`readFromJson`.
  [`AudioTrack::writeChannelToJson`](../../../Audionaut/Source/Engine/Group/AudioTrack.cpp#L116)
  writes `channel->data` into the element and then `"inserts"` / `"sends"` keys next to it;
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
                "inserts": [ { "type": "builtin:eq3", "enabled": true, "state": { "hp_hz": 80 } } ],
                "sends":   [ { "aux": 1, "level": 0.5, "pre_fader": false, "mute": false } ] } ],
"inserts": [ ],
"aux_buses":      [ { "id": 1, "name": "Reverb", "gain": 1.0, "balance": 0.0, "mute": false, "solo": false, "inserts": [ ] } ],
"master_inserts": [ ]
```

- Hosted plugin slot (phase 6): `{"type":"vst3:<uid>", "name", "manufacturer", "file",
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
- `MAX_AUDIO_CHANNELS` is 64 in Debug, 128 in Release; plan vectors are sized by the live lane
  count.
- `applyMuteAndSoloGains` scans lane data only
  ([`AudioBusRenderer.h:213`](../../../Audionaut/Source/Engine/Playback/AudioBusRenderer.h#L213));
  aux solo is OR-ed in from `AuxState` in phase 2.
- `AudioTrackContainer`'s constructor already takes 8 dependencies; the planner needs the
  container, so it is wired after construction with a setter (like
  `projectFileStore->setSerializer`).
- Effect CPU goes through the existing `DspLoadMeter`; the hidden bench (`[.][bench]`) gets an
  "N lanes with EQ + compressor" case.

### 2.7 CLI, MCP, UI touch points

- CLI verbs follow `runTrackPan`
  ([`TrackCommands.cpp:102`](../../../Audionaut/Source/Cli/Commands/TrackCommands.cpp#L102)) and
  the `getCliCommands()` table in `CliDispatch.cpp`. `info` extends the per-channel object in
  `ProjectSummary.cpp` with `inserts`/`sends` and adds `track_inserts`, `master_inserts`,
  `aux_buses`.
- MCP: `Tools/audionaut-mcp/index.js` `registerTool` wrappers; `test.mjs` asserts the exact
  sorted tool-name list and the count word, update both.
- New `.cpp` files: `<FILE>` entries in `Audionaut/Audionaut.jucer`, then Projucer resave;
  reconfigure CMake after adding files.
- UI (phase 4): `ChannelComponent` strip menu (`showStripMenu`) gets Add Insert ▸ / Sends ▸;
  `ChannelGroupHeaderComponent` gets track inserts; aux strips are appended in
  `ChannelsComponent`; generic `EffectEditorComponent` from `EffectParameterInfo`.

### 2.8 What is taken from Tracktion, what is not

| Tracktion | Audionaut | Why |
|---|---|---|
| General node DAG rebuilt from the model on every structural change | Fixed mixer topology + a plain-data `MixPlan` rebuilt on structural change | Three summing points; a DAG compiler buys nothing and costs a scheduler, cycle detection and node state carry-over |
| Lock-free object swap, deferred destruction on the non-RT side | Same idea with the existing `LockFreeContainer` + `isPulled()` retire list | Already proven in `VoiceSourceContainer` |
| Sends and returns are inert plugins; a bus is an integer | Sends are lane properties; the aux bus is a first-class object with its own strip | Console mental model; a bus with a name, chain and return fader needs an object anyway |
| Volume/pan/meter are plugins; pre/post = chain position | Fader/pan stay fixed stages; sends carry a pre/post flag | Keeps the existing renderer, undo and UI; pre/post is the only ordering users need |
| PDC via latency nodes inserted at every summing node by a transform pass | Same rule, computed arithmetically for the three known summing points | No graph transform needed |
| Multithreaded node player | Single audio thread (as today) | Lanes are independent until the mix stage; a per-track work split is a later optimisation if the bench says so |
| Automation streams under locks | Out of scope; parameters have stable ids and atomics, curves can come later | |

## 3. Implementation plan (one PR per phase, into develop)

### Phase 1: Effect interface, built-ins, inserts (lane, track, master), MixPlan

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
8. `AudioBusRenderer.h/.cpp`: pull plan, sub-block fix, lane inserts after `playback->process`,
   track inserts, master inserts before `masterGain.process`, clamping.
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
    (Trim −6 dB on one lane halves that lane in `LiveBlockDriver` render and in `bounceProject`;
    track insert sees both lanes; master insert affects main only, not direct outs; add/remove
    insert while playing stays click-free, `compareBuffers` vs bounce; undo/redo restores JSON and
    keeps effect instances; save/reopen; legacy project loads with empty chains; headless
    add-insert then export applies the insert; CLI envelope test).

### Phase 2: Sends and aux buses

1. `Effects/AuxBus.h/.cpp`, `AuxBusContainer.h/.cpp` (stable ids, `AuxState` atomics, insert
   chain), `SendEntry.h` (`SendState` atomics), `SendList`.
2. `AudioChannel`: `sends` member + JSON; `AudioTrackContainer` owns `AuxBusContainer`,
   `"aux_buses"` JSON, rebuild on add/remove.
3. `MixPlan`/`MixerPlanner`: aux buffers sized at build, per-lane sends, aux chains, aux solo
   folded into the solo rule.
4. `AudioBusRenderer.h`: post-fader taps from `stereoBlock` after `panners[i].process`, pre-fader
   taps with the balanced gain pair before `gains[i].process`, aux stage (inserts → return
   gain/balance/mute → meter) summed into `mainMixBlock` before master inserts.
5. Undo (`applyAsUndoableEdit` for structure, `UndoableSendAction` for levels), CLI
   `add-aux`/`remove-aux`/`set-aux`/`set-send --track --channel --aux --level [--pre-fader]
   [--mute]`, MCP, `ProjectSummary`, jucer, tests `SendRenderTests.cpp` (level −inf → aux silent;
   pre vs post fader with lane gain 0; aux insert Trim; mute/solo matrix incl. aux solo; JSON
   round trip; undo).

### Phase 3: Latency compensation and export alignment

- `getLatencySamples()` honoured; `PdcDelay` (fixed integer delay FIFO) per lane/aux/main/direct;
  arithmetic from 2.4 in `MixerPlanner::build`; delay state carried over by id; bypass delay in
  the slot.
- Scheduler reports `totalLatency` (playhead display offset); `bounceProject` pre-rolls by `L`
  and drops `L` samples, optional tail extension.
- Test-only `LatencyTestEffect(N)` registry entry: a click on lane A with latency 512 and lane B
  without lands on the same sample in the bounce; aux chain latency keeps dry/wet aligned; bypass
  toggle keeps alignment.

### Phase 4: Channel view UI

- `ChannelComponent`: "Inserts" rows (name, enable toggle, click → editor) and "Sends" rotaries;
  strip menu Add Insert ▸ (types from the registry) and Sends ▸.
- `ChannelGroupHeaderComponent`: track inserts entry. `AuxStripComponent`s appended in
  `ChannelsComponent`; master insert row.
- Generic `EffectEditorComponent` from `EffectParameterInfo` (sliders/toggles, drag → gesture
  undo), non-modal window per slot. Observes `updateChannelsAction` / `rebuildAll`; commands go
  straight to the engine.

### Phase 5: Remaining built-ins

- Limiter, Gate, Reverb, Delay, Chorus, Phaser; tail reporting for Reverb/Delay; bench case;
  manual chapter + `features.md` move the item out of *Not yet*.

### Phase 6: Plugin hosting (VST3 / AU; LV2 on Linux)

- Jucer: `JUCE_PLUGINHOST_VST3=1`, `JUCE_PLUGINHOST_AU=1` (macOS, `AudioUnit` framework),
  `JUCE_PLUGINHOST_LV2=1` (Linux). MAS build: sandbox allows AU only (sandbox-safe AUs); VST3
  hosting on the GitHub/DMG channel.
- `PluginRegistry` service (per process, injected, not static): `juce::AudioPluginFormatManager`
  + `KnownPluginList` persisted as XML in `Preferences::applicationDataDirectory()`; scanning in
  a child process (JUCE `ChildProcessCoordinator`, dead-man list so a crashing plugin is skipped
  on rescan).
- `HostedEffect : Effect` wrapping `juce::AudioPluginInstance`: bus layout negotiation to n
  channels, `processBlock` with an empty `MidiBuffer`, `getLatencySamples` → planner rebuild on
  change, `getStateInformation` base64 in the slot JSON, missing-plugin placeholder.
- Editor window (`createEditorIfNeeded`) in Interface; settings page for scan paths and rescan;
  CLI `scan-plugins`; hosted plugins process headless.

## 4. Verification (every phase)

```
cmake -B build -S Audionaut/Catch2Tests && cmake --build build -j8
./build/AudionautTests_artefacts/AudionautTests "[effects]"
./build/AudionautTests_artefacts/AudionautTests "[.][bench]"      # CPU before/after on the 64-lane project
```

Then the app build (jucer resave, Xcode Debug): play the bench project with inserts while adding
and removing effects (no clicks, DSP meter stable), undo/redo insert edits during playback. Hosted
agent path: an `add_insert` MCP call on the open document shows up as one undo step. Headless:
`audionaut-cli add-insert ... && audionaut-cli export ...` renders with the insert.
