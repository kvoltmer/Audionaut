//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudioBusRenderer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Recording/AudioRecorder.h"
#include "Engine/AudioSources/RenderTiming.h"

namespace audium
{

template <class SampleType>
AudioBusRenderer<SampleType>::AudioBusRenderer(std::shared_ptr<audium::Playback> playback_,
                                               std::shared_ptr<Recording> recording_,
                                               std::shared_ptr<RenderThreadPool> renderThreadPool_) :
    playback(playback_),
    recording(recording_),
    renderThreadPool(std::move(renderThreadPool_))
{
    setMasterGain(1.f);
    for (auto& cost : jobCost)
        cost.store (0.0f, std::memory_order_relaxed);
}

template <class SampleType>
void AudioBusRenderer<SampleType>::setNumAudioBusChannels(int numChannels)
{
    // called per block from the audio thread: only narrows the view over
    // the MAX_AUDIO_CHANNELS allocation prepareToPlay made (avoidReallocating)
    if (numChannels != audioBus.getNumChannels()) {
        audioBus.setSize(numChannels, audioBus.getNumSamples(), false, false, true);

        // whether any channel is soloed depends on which channels the bus renders
        applyMuteAndSoloGains();
    }
}

template <class SampleType>
void AudioBusRenderer<SampleType>::prepareToPlay (int samplesPerBlockExpected, double sampleRate_)
{
    sampleRate = sampleRate_;
    recording->setSampleRate(sampleRate);
    // sized once for every channel the bus can carry; setNumAudioBusChannels
    // then never allocates on the audio thread. The channel count is
    // re-narrowed on the next block.
    audioBus.setSize(MAX_AUDIO_CHANNELS, samplesPerBlockExpected);
    
    mainMixBuffer.setSize(2, samplesPerBlockExpected);
    // a partial per job: at most one job per channel
    trackOutput.setSize(2 * MAX_AUDIO_CHANNELS, samplesPerBlockExpected);
    panOutput.setSize(2 * maxWorkerSlots, samplesPerBlockExpected);
    voiceOutput.setSize(maxWorkerSlots, samplesPerBlockExpected);

    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize    = samplesPerBlockExpected;
    spec.sampleRate          = sampleRate;
    
    for (auto i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
        
        spec.numChannels = 2;
        panners[i].prepare(spec);
        
        spec.numChannels = 1;
        gains[i].setRampDurationSeconds(0.01);
        gains[i].prepare(spec); 
    }
    
    spec.numChannels = 2;
    masterGain.setRampDurationSeconds(0.01);
    masterGain.prepare(spec);
}

template <class SampleType>
void AudioBusRenderer<SampleType>::process (const ProcessContextNonReplacing<SampleType>& context) noexcept
{
    auto outputBlock = context.getOutputBlock();
    outputBlock.clear();
    const auto outputChannels = static_cast<int> (outputBlock.getNumChannels());

    // the buffers are sized by prepareToPlay; a device handing over a
    // longer block than announced is clamped, never read out of bounds
    jassert (static_cast<int> (outputBlock.getNumSamples()) <= audioBus.getNumSamples());
    const auto numSamples = std::min (static_cast<int> (outputBlock.getNumSamples()), audioBus.getNumSamples());

    const auto audioBusChannels = audioBus.getNumChannels();
    if (audioBusChannels <= 0 || numSamples <= 0)
        return;

    // every view below covers this block's length, not the prepared one
    auto audioBusBlock = AudioBlock<SampleType> (audioBus).getSubBlock (0, static_cast<size_t> (numSamples));
    audioBusBlock.clear();

    buildJobTable (audioBusChannels);
    bucketVoices (audioBusChannels);

    BlockContext ctx;
    ctx.input = context.getInputBlock();
    ctx.audioBus = audioBusBlock;
    ctx.trackOutput = AudioBlock<SampleType> (trackOutput).getSubBlock (0, static_cast<size_t> (numSamples));
    ctx.panOutput = AudioBlock<SampleType> (panOutput).getSubBlock (0, static_cast<size_t> (numSamples));
    ctx.voiceOutput = AudioBlock<float> (voiceOutput).getSubBlock (0, static_cast<size_t> (numSamples));
    ctx.numSamples = numSamples;
    ctx.stereoMain = outputChannels >= 2 && ! stemExport.load();

    if (sampleRate > 0.0)
        ctx.blockTicks = static_cast<juce::int64> (numSamples / sampleRate * static_cast<double> (juce::Time::getHighResolutionTicksPerSecond()));

    if (ctx.blockTicks > 0 && trackJobSharesEnabled.load (std::memory_order_relaxed))
    {
        ctx.shareTable = 1 - publishedShareTable.load (std::memory_order_relaxed);
        jobShareTables[ctx.shareTable].sequence.fetch_add (1, std::memory_order_relaxed);   // odd: being filled
        std::atomic_thread_fence (std::memory_order_release);   // ... and visibly so before any store into it
        for (auto& slot : jobShareTables[ctx.shareTable].share)
            for (auto j = 0; j < numJobs; ++j)
                slot[j].store (0.0f, std::memory_order_relaxed);
    }

   #if JUCE_DEBUG
    ++blockCounter;
   #endif

    // stage A: one job per track, heaviest first
    auto trackJob = [this, &ctx] (int slot, int workerId) noexcept
    {
        const auto job = jobOrder[slot];
        renderTrack (jobs[job], job, workerId, ctx);
    };

    // the pool serves the device callback; a bounce renders serially on
    // the thread that drives it (same code, same samples)
    if (renderThreadPool != nullptr && renderThreadPool->isRunning() && ! RenderTiming::isOffline())
        renderThreadPool->run (numJobs, trackJob);
    else
        for (auto slot = 0; slot < numJobs; ++slot)
            trackJob (slot, 0);

    if (ctx.shareTable >= 0)
    {
        jobShareTables[ctx.shareTable].numJobs.store (numJobs, std::memory_order_relaxed);
        jobShareTables[ctx.shareTable].sequence.fetch_add (1, std::memory_order_release);   // even: complete
        publishedShareTable.store (ctx.shareTable, std::memory_order_release);
    }
    else
    {
        jobShareTables[publishedShareTable.load (std::memory_order_relaxed)].numJobs.store (0, std::memory_order_relaxed);
    }

    // stage C: the mix, in track order
    renderMaster (outputBlock, outputChannels, ctx);
}

template <class SampleType>
int AudioBusRenderer<SampleType>::readTrackJobShares (TrackJobShare* out, int capacity) const noexcept
{
    for (auto attempt = 0; attempt < 4; ++attempt)
    {
        const auto& table = jobShareTables[publishedShareTable.load (std::memory_order_acquire)];
        const auto sequence = table.sequence.load (std::memory_order_acquire);
        if (sequence & 1u)
            continue;   // the audio thread came back to it: the other one is published by now

        const auto count = std::min (table.numJobs.load (std::memory_order_relaxed), capacity);
        for (auto j = 0; j < count; ++j)
        {
            out[j].trackId = table.trackId[j].load (std::memory_order_relaxed);
            for (auto slot = 0; slot < RenderLoadMeter::maxThreadSlots; ++slot)
                out[j].share[slot] = table.share[slot][j].load (std::memory_order_relaxed);
        }

        std::atomic_thread_fence (std::memory_order_acquire);
        if (table.sequence.load (std::memory_order_relaxed) == sequence)
            return count;
    }

    return 0;
}

template <class SampleType>
void AudioBusRenderer<SampleType>::setPan(const int channelNumber, const SampleType newPan)
{
    if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
        panners[channelNumber].setPan(newPan);
    }
}

template <class SampleType>
void AudioBusRenderer<SampleType>::setGain(const int channelNumber, const SampleType newGain)
{
    if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
        if (audioChannelData[channelNumber].solo)
            gains[channelNumber].setGainLinear(newGain);
        else if (!audioChannelData[channelNumber].mute)
            gains[channelNumber].setGainLinear(newGain);

        audioChannelData[channelNumber].gain = newGain;
    }
}

template <class SampleType>
void AudioBusRenderer<SampleType>::setMute(const int channelNumber, const bool bMute)
{
    if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
        audioChannelData[channelNumber].mute = bMute;
        if (!audioChannelData[channelNumber].solo)
            gains[channelNumber].setGainLinear(bMute ? 0.f : audioChannelData[channelNumber].gain);
    }
}

template <class SampleType>
void AudioBusRenderer<SampleType>::setSolo(const int channelNumber, const bool bSolo)
{
    if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
        audioChannelData[channelNumber].solo = bSolo;
        applyMuteAndSoloGains();
    }
}

template <class SampleType>
void AudioBusRenderer<SampleType>::applyMuteAndSoloGains()
{
    auto anySolo = false;
    for (auto c = 0; c < audioBus.getNumChannels(); ++c) {
        if (audioChannelData[c].solo) {
            anySolo = true;
            break;
        }
    }

    for (auto c = 0; c < MAX_AUDIO_CHANNELS; ++c) {
        if (anySolo) {
            if (audioChannelData[c].solo)
                gains[c].setGainLinear(audioChannelData[c].gain);
            else
                gains[c].setGainLinear(0.f);
        }
        else {
            // mute stats apply
            gains[c].setGainLinear(audioChannelData[c].mute ? 0.f : audioChannelData[c].gain);
        }
    }
}

template <class SampleType>
void AudioBusRenderer<SampleType>::setChannelData(const int channelNumber, const AudioChannelData data)
{
    if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
        audioChannelData[channelNumber] = data;
        setPan(channelNumber, data.pan);
        
        setGain(channelNumber, data.gain);
        setMute(channelNumber, data.mute);
        setSolo(channelNumber, data.solo);
    }
}

template <class SampleType>
const AudioChannelData AudioBusRenderer<SampleType>::getChannelData(const int channelNumber) const
{
    if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
        return audioChannelData[channelNumber];
    }
    return AudioChannelData();
}

template <class SampleType>
void AudioBusRenderer<SampleType>::setMasterGain(const float newGain)
{
    masterGain.setGainLinear(newGain);
}

template <class SampleType>
const float AudioBusRenderer<SampleType>::getChannelLevel(const int channelNumber) const
{
    if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS)
        return channelLevel[channelNumber].load();

    return 0.f;
}

template <class SampleType>
const float AudioBusRenderer<SampleType>::getRecordingLevel(const int channelNumber) const
{
    if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS)
        return recordingLevel[channelNumber].load();

    return 0.f;
}

template <class SampleType>
const float AudioBusRenderer<SampleType>::getMasterLevel(const int channelNumber) const
{
    if (channelNumber >= 0 && channelNumber < 2)
        return masterLevel[channelNumber].load();

    return 0.f;
}

template <class SampleType>
void AudioBusRenderer<SampleType>::resetGains()
{
    // stupid code to avoid smoothing when start processing (start playing)
    for (auto i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
        auto rampDuration = gains[i].getRampDurationSeconds();
        gains[i].setRampDurationSeconds(0.0);
        gains[i].setGainLinear(gains[i].getGainLinear());
        gains[i].setRampDurationSeconds(rampDuration);
    }
    
    auto rampDuration = masterGain.getRampDurationSeconds();
    masterGain.setRampDurationSeconds(0.0);
    masterGain.setGainLinear(masterGain.getGainLinear());
    masterGain.setRampDurationSeconds(rampDuration);
}

template <class SampleType>
void AudioBusRenderer<SampleType>::buildJobTable (int audioBusChannels) noexcept
{
    numJobs = 0;
    for (auto busChannel = 0; busChannel < audioBusChannels; ++busChannel)
    {
        if (busChannel == 0 || audioChannelData[busChannel].trackId != audioChannelData[busChannel - 1].trackId)
        {
            auto& job = jobs[numJobs++];
            job.firstChannel = busChannel;
            job.numChannels = 0;
        }
        ++jobs[numJobs - 1].numChannels;
        channelToJob[busChannel] = numJobs - 1;
    }

    // insertion sort by cost, descending: the order is nearly stable
    // from block to block, so this is a pass over the table
    for (auto j = 0; j < numJobs; ++j)
    {
        const auto cost = jobCost[jobs[j].firstChannel].load (std::memory_order_relaxed);
        auto i = j;
        while (i > 0 && jobCost[jobs[jobOrder[i - 1]].firstChannel].load (std::memory_order_relaxed) < cost)
        {
            jobOrder[i] = jobOrder[i - 1];
            --i;
        }
        jobOrder[i] = j;
    }
}

template <class SampleType>
void AudioBusRenderer<SampleType>::bucketVoices (int audioBusChannels) noexcept
{
    // Each voice's channel is read once: the message thread may move a
    // voice to another channel (VoiceSource::applyChannelMapping) at any
    // time, and the fill pass must count the voice where the count pass did.
    int count[MAX_AUDIO_CHANNELS] = {};   // per job

    for (auto v = 0; v < MAX_VOICES; ++v)
    {
        auto busChannel = playback->getActiveVoiceChannel (v);

        if (busChannel >= audioBusChannels)
        {
            playback->stopVoiceNow (v);   // see the declaration
            busChannel = -1;
        }

        voiceChannel[v] = busChannel;
        if (busChannel >= 0)
            ++count[channelToJob[busChannel]];
    }

    auto begin = 0;
    for (auto j = 0; j < numJobs; ++j)
    {
        jobs[j].voiceBegin = begin;
        jobs[j].voiceCount = 0;
        begin += count[j];
    }

    for (auto v = 0; v < MAX_VOICES; ++v)
    {
        if (voiceChannel[v] < 0)
            continue;

        auto& job = jobs[channelToJob[voiceChannel[v]]];
        voiceOrder[job.voiceBegin + job.voiceCount++] = v;
    }
}

template <class SampleType>
void AudioBusRenderer<SampleType>::addVoiceOutput (SampleType* busChannel, const float* output, int numSamples) noexcept
{
    if constexpr (std::is_same_v<SampleType, float>)
        juce::FloatVectorOperations::add (busChannel, output, numSamples);
    else
        for (auto s = 0; s < numSamples; ++s)
            busChannel[s] += static_cast<SampleType> (output[s]);
}

template <class SampleType>
void AudioBusRenderer<SampleType>::renderTrack (TrackJob& job, int jobIndex, int workerId, const BlockContext& ctx) noexcept
{
    const auto startTicks = juce::Time::getHighResolutionTicks();
    const auto& audioBusBlock = ctx.audioBus;
    const auto channelEnd = job.firstChannel + job.numChannels;

    renderInput (job, workerId, ctx, audioBusBlock);

    // the track's voices, each into the worker's voice output row, added to its bus channel
    auto* output = ctx.voiceOutput.getChannelPointer (static_cast<size_t> (workerId));
    for (auto i = 0; i < job.voiceCount; ++i)
    {
        const auto busChannel = playback->renderVoice (voiceOrder[job.voiceBegin + i], output, ctx.numSamples);
        if (busChannel >= job.firstChannel && busChannel < channelEnd)
            addVoiceOutput (audioBusBlock.getChannelPointer (static_cast<size_t> (busChannel)), output, ctx.numSamples);
    }

    // gain and metering per channel
    for (auto k = job.firstChannel; k < channelEnd; ++k) {
        auto channelBlock = audioBusBlock.getSingleChannelBlock (static_cast<size_t> (k));
        ProcessContextReplacing<SampleType> gainContext( channelBlock );
        gains[k].process(gainContext);
        channelLevel[k] = std::abs(channelBlock.findMinAndMax().getEnd());
    }

    if (ctx.stereoMain)
        panIntoPartial (job, jobIndex, workerId, ctx, audioBusBlock);

   #if JUCE_DEBUG
    burnSimulatedLoad (job, ctx);
   #endif

    // the cost that orders the next block's jobs: a running average, so
    // one slow block (a stretch voice priming) does not reorder everything
    const auto elapsed = static_cast<float> (juce::Time::getHighResolutionTicks() - startTicks);
    auto& cost = jobCost[job.firstChannel];
    cost.store (cost.load (std::memory_order_relaxed) * 0.8f + elapsed * 0.2f, std::memory_order_relaxed);

    if (ctx.shareTable >= 0)
    {
        auto& table = jobShareTables[ctx.shareTable];
        table.share[workerId][jobIndex].store (elapsed / static_cast<float> (ctx.blockTicks), std::memory_order_relaxed);
        table.trackId[jobIndex].store (audioChannelData[job.firstChannel].trackId, std::memory_order_relaxed);
    }
}

template <class SampleType>
void AudioBusRenderer<SampleType>::renderInput (const TrackJob& job, int workerId, const BlockContext& ctx, const AudioBlock<SampleType>& audioBusBlock) noexcept
{
    const auto inputChannels = static_cast<int> (ctx.input.getNumChannels());
    const auto channelEnd = job.firstChannel + job.numChannels;

    // each bus channel is fed by its effective hardware input
    for (auto k = job.firstChannel; k < channelEnd; ++k) {
        const auto effectiveInput = AudioChannelData::effectiveInputChannel(audioChannelData[k]);
        if (effectiveInput < 0 || effectiveInput >= inputChannels)
            continue;

        auto input = ctx.input.getSingleChannelBlock (static_cast<size_t> (effectiveInput));

        if (audioChannelData[k].monitor) {
            audioBusBlock.getSingleChannelBlock (static_cast<size_t> (k)).add (input);
        }

        if (audioChannelData[k].record) {
            recordingLevel[k] = std::abs (input.findMinAndMax().getEnd());

            if (auto* recorder = recording->getAudioThreadRecorder(k)) {
                if constexpr (std::is_same_v<SampleType, float>) {
                    auto output = audioBusBlock.getSingleChannelBlock (static_cast<size_t> (k));
                    ProcessContextNonReplacing<float> recContext(input, output);
                    recorder->process( recContext );
                }
                else {
                    // the recorder writes float: convert into the worker's
                    // voice output row, which the voices use only after this
                    auto* recorderInput = ctx.voiceOutput.getChannelPointer (static_cast<size_t> (workerId));
                    const auto* src = input.getChannelPointer (0);
                    for (auto s = 0; s < ctx.numSamples; ++s)
                        recorderInput[s] = static_cast<float> (src[s]);
                    float* channels[] = { recorderInput };
                    AudioBlock<float> recorderBlock (channels, 1, static_cast<size_t> (ctx.numSamples));
                    ProcessContextReplacing<float> recContext (recorderBlock);
                    recorder->process (recContext);
                }
            }
        }
    }
}

template <class SampleType>
void AudioBusRenderer<SampleType>::panIntoPartial (const TrackJob& job, int jobIndex, int workerId, const BlockContext& ctx, const AudioBlock<SampleType>& audioBusBlock) noexcept
{
    const auto channelEnd = job.firstChannel + job.numChannels;

    auto partial = ctx.trackOutput.getSubsetChannelBlock (static_cast<size_t> (2 * jobIndex), 2);
    partial.clear();
    auto panned = ctx.panOutput.getSubsetChannelBlock (static_cast<size_t> (2 * workerId), 2);

    for (auto k = job.firstChannel; k < channelEnd; ++k) {
        if (audioChannelData[k].outputChannel >= 0)
            continue;   // a direct out: taken post-gain in stage C

        auto channelBlock = audioBusBlock.getSingleChannelBlock (static_cast<size_t> (k));
        panned.clear();
        ProcessContextNonReplacing<SampleType> panContext (channelBlock, panned);
        panners[k].process (panContext);
        partial.add (panned);
    }
}

#if JUCE_DEBUG
template <class SampleType>
void AudioBusRenderer<SampleType>::burnSimulatedLoad (const TrackJob& job, const BlockContext& ctx) noexcept
{
    const auto trackId = audioChannelData[job.firstChannel].trackId;
    if (trackId < 0 || trackId >= MAX_AUDIO_CHANNELS)
        return;

    const auto share = simulatedLoad[trackId].load (std::memory_order_relaxed);
    if (share <= 0.0f)
        return;

    // a bursty load lands on one block in eight
    if (simulatedLoadBursty[trackId].load (std::memory_order_relaxed) && (blockCounter & 7u) != 0)
        return;

    const auto until = juce::Time::getHighResolutionTicks() + static_cast<juce::int64> (share * static_cast<float> (ctx.blockTicks));
    while (juce::Time::getHighResolutionTicks() < until)
        ;
}
#endif

template <class SampleType>
void AudioBusRenderer<SampleType>::renderMaster (AudioBlock<SampleType>& outputBlock, int outputChannels, const BlockContext& ctx) noexcept
{
    const auto numSamples = static_cast<size_t> (ctx.numSamples);
    const auto audioBusChannels = static_cast<int> (ctx.audioBus.getNumChannels());
    const auto& audioBusBlock = ctx.audioBus;

    if (stemExport.load()) {
        // offline bounce: identity copy, one output channel per bus channel
        for (auto c = 0; c < std::min (outputChannels, audioBusChannels); c++) {
            outputBlock.getSingleChannelBlock (static_cast<size_t> (c)).add (audioBusBlock.getSingleChannelBlock (static_cast<size_t> (c)));
        }
        return;
    }

    auto mainMixBlock = AudioBlock<SampleType> (mainMixBuffer).getSubBlock (0, numSamples);
    if (ctx.stereoMain) {
        mainMixBlock.clear();
        for (auto j = 0; j < numJobs; ++j)
            mainMixBlock.add (ctx.trackOutput.getSubsetChannelBlock (static_cast<size_t> (2 * j), 2));
    }

    for (auto i = 0; i < audioBusChannels; i++) {
        auto channelBlock = audioBusBlock.getSingleChannelBlock (static_cast<size_t> (i));
        const auto destination = audioChannelData[i].outputChannel;

        if (destination < 0) {
            if (outputChannels == 1) {
                // mono device: sum the main mix into the single output
                outputBlock.getSingleChannelBlock (0).add (channelBlock);
            }
        }
        else if (destination < outputChannels) {
            // direct mono out: post channel-gain, no pan, no master gain
            outputBlock.getSingleChannelBlock (static_cast<size_t> (destination)).add (channelBlock);
        }
        // else: routed beyond the current device's outputs -> dropped
    }

    if (ctx.stereoMain) {
        // master gain + level metering apply to the main mix only
        ProcessContextReplacing<SampleType> gainContext(mainMixBlock);
        masterGain.process(gainContext);

        for (auto m = 0; m < 2; ++m) {
            auto minmax = mainMixBlock.getSingleChannelBlock (static_cast<size_t> (m)).findMinAndMax();
            masterLevel[m].store( std::abs(minmax.getEnd()) );
            outputBlock.getSingleChannelBlock (static_cast<size_t> (m)).add (mainMixBlock.getSingleChannelBlock (static_cast<size_t> (m)));
        }
    }
}

template class AudioBusRenderer<float>;
template class AudioBusRenderer<double>;

} // namespace audium
