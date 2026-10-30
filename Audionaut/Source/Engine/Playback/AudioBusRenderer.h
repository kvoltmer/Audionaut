//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "PlaybackDefines.h"
#include "RenderThreadPool.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Playback/Playback.h"
#include "Engine/Channel/AudioChannelData.h"
#include "Engine/Recording/Recording.h"

using namespace juce::dsp;

namespace audium
{
class AudioThumbnail;

/**
 * One track job's share of the last block, per render thread slot (0 = the
 * audio thread, 1.. the workers): what the Processing Load window colours
 * the thread bars with when asked to. A job runs on one slot per block, so
 * all but one entry are zero.
 */
struct TrackJobShare
{
    int trackId = -1;
    float share[RenderLoadMeter::maxThreadSlots] = {};
};

/**
 * @class AudioBusRenderer
 * @brief Handles audio bus processing, including channel management, panning, gain, and playback.
 *
 * The bus is a flat array of mono channels; every AudioChannel of every track
 * owns one of them. A block is rendered in two stages:
 *
 *  - Stage A, one job per track (the run of bus channels with the track's id):
 *    the bus channels' input monitoring and recording, the track's voices, channel
 *    gain and metering, and the pan of each bus channel into the track's own
 *    stereo partial (`trackOutput`). Jobs are independent - a job touches only
 *    its channels, its partial and its voices - so the RenderThreadPool runs
 *    them concurrently, longest first.
 *  - Stage C, on the calling thread: the partials are summed in track order
 *    into the main mix, direct outs and stem copies are taken from the
 *    post-gain bus channels, then master gain and metering.
 *
 * The sums always run in track order on one thread, so the output is the
 * same for any number of workers, including none (the serial path, which is
 * what offline bounces use). Stage B (aux buses) is reserved for sends.
 *
 * @tparam SampleType The type of audio sample (float or double; voices render in float).
 */
template <class SampleType>
class AudioBusRenderer {

public:
    /**
     * @param renderThreadPool_ The pool that runs the track jobs of a live
     *        block; serial while it is not running. Shared with
     *        AudioBusInterface, which starts and stops it with the device.
     */
    AudioBusRenderer(std::shared_ptr<audium::Playback> playback_,
                     std::shared_ptr<Recording> recording_,
                     std::shared_ptr<RenderThreadPool> renderThreadPool_);

    ~AudioBusRenderer() = default;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);

    void setNumAudioBusChannels(int numChannels);

    /** The bus channel count the last block rendered with. */
    int getNumAudioBusChannels() const noexcept { return audioBus.getNumChannels(); }

    /**
     * Renders one block: the device input into the monitored and recorded
     * bus channels, the voices, gain, pan and the mix into the context's output
     * block (which is cleared first). Audio thread, or the thread driving
     * an offline bounce.
     */
    void process (const ProcessContextNonReplacing<SampleType>& context) noexcept;

    void setPan(const int channelNumber, const SampleType newPan);

    void setGain(const int channelNumber, const SampleType newGain);

    void setMute(const int channelNumber, const bool bMute);

    void setSolo(const int channelNumber, const bool bSolo);

    /**
     * Sets every channel's gain from its mute and solo state: with any bus
     * channel soloed only the soloed ones play, otherwise the unmuted ones.
     * Solo is looked for among the channels the bus renders, so this runs
     * again whenever that count changes (setNumAudioBusChannels) - a solo
     * applied before the bus has grown to its channel, as on project load,
     * takes effect once it has.
     */
    void applyMuteAndSoloGains();

    void setChannelData(const int channelNumber, const AudioChannelData data);

    const AudioChannelData getChannelData(const int channelNumber) const;

    void setMasterGain(const float newGain);

    const float getChannelLevel(const int channelNumber) const;

    const float getRecordingLevel(const int channelNumber) const;

    const float getMasterLevel(const int channelNumber) const;

    std::shared_ptr<Recording> getRecording() const { return recording; }

    std::shared_ptr<Playback> getPlayback() const { return playback; }

    void setRecordEnabled(int channelNumber, bool bEnabled) { audioChannelData[channelNumber].record = bEnabled; }

    /**
     * @brief Enables the offline bounce mapping: identity copy of bus channels to output channels,
     * bypassing output routing. Only set while the live device callback is bypassed.
     */
    void setStemExport(bool bStemExport) { stemExport.store(bStemExport); }

    void resetGains();

    const Gain<SampleType>& getGain(int channel) { return gains[channel]; }

    /** The track jobs the last block was split into (tests). */
    int getNumTrackJobs() const noexcept { return numJobs; }

    /**
     * Records per job and thread slot how much of the block period it took,
     * for readTrackJobShares(). Off by default: it is a curiosity for the
     * Processing Load window, not something every block should pay for.
     */
    void setTrackJobSharesEnabled (bool enabled) noexcept { trackJobSharesEnabled.store (enabled, std::memory_order_relaxed); }

    /**
     * Message thread: copies the last block's job shares into `out`, at most
     * `capacity` of them, and returns how many jobs the block had. Zero
     * while recording is off, or when the table was refilled under the copy
     * every time it looked (it tries a few times). Values of one block, not
     * smoothed, and a job may move to another slot in the next.
     */
    int readTrackJobShares (TrackJobShare* out, int capacity) const noexcept;

   #if JUCE_DEBUG
    /**
     * Debug builds only: makes the track's job burn `share` of the block
     * period on top of its real work, every block or (bursty) one block in
     * eight, to try overload scenarios against the pool and the Processing
     * Load window. Track ids as the channel data carries them; 0 = off.
     */
    void setSimulatedTrackLoad (int trackId, float share, bool bursty) noexcept
    {
        if (trackId >= 0 && trackId < MAX_AUDIO_CHANNELS)
        {
            simulatedLoad[trackId].store (juce::jmax (0.0f, share), std::memory_order_relaxed);
            simulatedLoadBursty[trackId].store (bursty, std::memory_order_relaxed);
        }
    }
   #endif

private:
    /// One stage-A job: a track's run of bus channels and the voices feeding them.
    struct TrackJob
    {
        int firstChannel = 0;
        int numChannels = 0;
        int voiceBegin = 0;   ///< into voiceOrder
        int voiceCount = 0;
    };

    /**
     * What every job of a block shares. The buffer views are built once on
     * the calling thread: a job writes only its own channels through them,
     * and never touches the AudioBuffers themselves (a writable accessor
     * clears the buffer's non-atomic "is clear" flag, a race between jobs).
     */
    struct BlockContext
    {
        AudioBlock<const SampleType> input;
        AudioBlock<SampleType> audioBus;       ///< the bus channels, the block's length
        AudioBlock<SampleType> trackOutput;    ///< the stereo partials, one pair per job
        AudioBlock<SampleType> panOutput;      ///< one stereo pair per worker slot
        AudioBlock<float> voiceOutput;         ///< one mono row per worker slot
        int numSamples = 0;
        bool stereoMain = false;   ///< bus channels pan into the main mix (a stereo device, no stem export)
        juce::int64 blockTicks = 0;   ///< the block period in high-resolution ticks
        int shareTable = -1;          ///< the job share table this block fills, -1 = not recording
    };

    /**
     * The jobs of this block: one per run of bus channels with the same track id
     * (a track's channels are contiguous, AudioChannel::commitChannelData),
     * ordered longest-first by the measured cost of the previous blocks.
     */
    void buildJobTable (int audioBusChannels) noexcept;

    /**
     * Groups the active voices by the track job that will render them, so
     * each job knows which voices are its own. It runs once per block on
     * the audio thread, before the jobs are dispatched: a counting sort of
     * the voices by job, into voiceOrder. A voice whose channel the bus
     * does not render this block is stopped without a render: either its
     * track was just removed (the stop requested on the message thread
     * completes here, there is no channel to fade into) or its track's
     * channels are not published yet (the scheduler starts it again next
     * block, from the right position).
     */
    void bucketVoices (int audioBusChannels) noexcept;

    /// Adds a voice's float output to a bus channel, converting when the bus is not float.
    static void addVoiceOutput (SampleType* busChannel, const float* output, int numSamples) noexcept;


    /**
     * Stage A for one track: its channels' input monitoring and recording, its
     * voices, channel gain and metering, and the pan into its stereo partial.
     * Writes only the job's channels, its partial, their meters and the
     * worker's own output rows, so jobs run concurrently.
     */
    void renderTrack (TrackJob& job, int jobIndex, int workerId, const BlockContext& ctx) noexcept;

    /// The job's channels' input: monitored input added to the bus, armed channels metered and recorded.
    void renderInput (const TrackJob& job, int workerId, const BlockContext& ctx, AudioBlock<SampleType>& audioBusBlock) noexcept;

    /// The job's main-mix channels panned and summed into the track's stereo partial.
    void panIntoPartial (const TrackJob& job, int jobIndex, int workerId, const BlockContext& ctx, AudioBlock<SampleType>& audioBusBlock) noexcept;

    /**
     * Stage C: the partials summed in track order into the main mix, the
     * direct outs and stem copies from the post-gain bus channels, master gain
     * and metering. Calling thread only.
     */
    void renderMaster (AudioBlock<SampleType>& outputBlock, int outputChannels, const BlockContext& ctx) noexcept;

    static constexpr int maxWorkerSlots = RenderThreadPool::maxWorkers + 1;   // the calling thread is slot 0

    std::shared_ptr<audium::Playback> playback;

    std::shared_ptr<Recording> recording;

    std::shared_ptr<RenderThreadPool> renderThreadPool;

    juce::AudioBuffer<SampleType> audioBus;
    juce::AudioBuffer<SampleType> mainMixBuffer;
    juce::AudioBuffer<SampleType> trackOutput;   ///< a track's output: stereo partial per job, channels 2j, 2j+1
    juce::AudioBuffer<SampleType> panOutput;     ///< a channel's panned output: one stereo pair per worker slot
    juce::AudioBuffer<float> voiceOutput;        ///< a voice's output: one mono row per worker slot

    TrackJob jobs[MAX_AUDIO_CHANNELS];
    int numJobs = 0;
    int jobOrder[MAX_AUDIO_CHANNELS] = {};       ///< job indices, heaviest first
    int channelToJob[MAX_AUDIO_CHANNELS] = {};
    int voiceOrder[MAX_VOICES] = {};             ///< voice indices grouped by job
    int voiceChannel[MAX_VOICES] = {};           ///< each voice's bus channel as bucketVoices read it, -1 = none
    std::atomic<float> jobCost[MAX_AUDIO_CHANNELS];   ///< ticks per block, by the job's first channel

    // The per-slot job shares the Processing Load window reads, written by
    // whichever thread ran the job. Two tables: a block clears and fills the
    // one that is not published and publishes it when its jobs are done, so
    // a reader never sees a block half filled. The sequence is odd while the
    // table is being filled: a reader that was held up long enough for the
    // audio thread to come back to the table it is copying sees it change
    // and tries again.
    struct JobShareTable
    {
        std::atomic<std::uint32_t> sequence { 0 };
        std::atomic<int> numJobs { 0 };
        std::atomic<int> trackId[MAX_AUDIO_CHANNELS] = {};
        std::atomic<float> share[RenderLoadMeter::maxThreadSlots][MAX_AUDIO_CHANNELS] = {};
    };

    std::atomic<bool> trackJobSharesEnabled { false };
    JobShareTable jobShareTables[2];
    std::atomic<int> publishedShareTable { 0 };

   #if JUCE_DEBUG
    /// Burns the track's simulated load at the end of its job, so it counts as the job's own time.
    void burnSimulatedLoad (const TrackJob& job, const BlockContext& ctx) noexcept;

    std::atomic<float> simulatedLoad[MAX_AUDIO_CHANNELS] = {};    ///< by track id, share of the block period
    std::atomic<bool> simulatedLoadBursty[MAX_AUDIO_CHANNELS] = {};
    std::uint32_t blockCounter = 0;                                 ///< audio thread only, for the bursts
   #endif

    std::atomic<bool> stemExport { false };

    Panner<SampleType> panners[MAX_AUDIO_CHANNELS];
    Gain<SampleType> gains[MAX_AUDIO_CHANNELS];
    Gain<SampleType> masterGain;

    std::atomic<float> masterLevel[2];
    std::atomic<float> channelLevel[MAX_AUDIO_CHANNELS];
    std::atomic<float> recordingLevel[MAX_AUDIO_CHANNELS];

    AudioChannelData audioChannelData[MAX_AUDIO_CHANNELS];

    double sampleRate = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioBusRenderer)

};

} // namespace audium
