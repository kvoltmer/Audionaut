//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <JuceHeader.h>
#include <farbot/fifo.hpp>

#include "Engine/TimeContext.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Link/LinkEngine.hpp"
#include "Engine/PlayList/PlayListSchedulerData.h"
#include "Engine/Core/AudioClipContainer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Export/ExportAudioConfig.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/PlayList/TransportLoop.h"

namespace audium {

class PlayListContainer;
class PlayListItem;
class VoiceSourceContainer;
struct ClipFadeSpec;
class AudioResourceContainer;
class Playback;
class DspClip;

/**
 * \class PlayListScheduler
 * \brief Manages the scheduling and playback of playlist items in an audio application.
 *
 * The `PlayListScheduler` class is responsible for handling the sequencing, playback,
 * and management of playlist items across multiple audio tracks. It integrates with
 * various components such as tempo providers, transport sources, and playback engines
 * to ensure synchronized audio playback. The class also supports editing modes,
 * transport following, and exporting audio to files.
 */
class PlayListScheduler : public juce::ChangeListener
{
    
    
public:
    PlayListScheduler(std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
                      std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                      std::shared_ptr<TempoProvider> tempoProvider_,
                      std::shared_ptr<audium::LinkEngine> linkEngine_,
                      std::shared_ptr<audium::AudioClipContainer> audioClipContainer_,
                      std::shared_ptr<VoiceSourceContainer> voiceSourceContainer_,
                      std::shared_ptr<audium::Playback> playback_,
                      std::shared_ptr<AudioBusInterface> audioBusInterface_,
                      std::shared_ptr<TransportLoop> transportLoop_) :
        audioTrackContainer(audioTrackContainer_),
        audioResourceContainer(audioResourceContainer_),
        tempoProvider(tempoProvider_),
        linkEngine(linkEngine_),
        audioClipContainer(audioClipContainer_),
        voiceSourceContainer(voiceSourceContainer_),
        playback(playback_),
        audioBusInterface(audioBusInterface_),
        transportLoop(transportLoop_)
    {        
        audioTrackContainer->addChangeListener(this);
    }
    
    ~PlayListScheduler() override
    {
        audioTrackContainer->removeChangeListener(this);
    }
    
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        commitPlayListData();
    }
    
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate);
    
    /// The file position (seconds) a voice for the clip starts from when
    /// the transport is at transportPosition - what scheduleClip seeks to,
    /// and what the standby prime targets ahead of a loop wrap.
    static double restartFilePosition(const audium::DspClip &clip,
                                      const ClipFadeSpec &spec,
                                      double transportPosition);

    /// Runs the standby prime of a stretched voice over the blocks before
    /// its next start - the clip's start or the loop wrap, whichever is
    /// nearer; a no-op outside that window or for other clips.
    void primeStandbyForUpcomingStart(const audium::DspClip &clip,
                                      VoiceSource* voiceSource,
                                      double transportPosition,
                                      const TransportLoop::LoopResult &loopResult,
                                      int numSamples);

    /// Primes, whole and on the calling (message) thread, the standbys of
    /// the stretched clips the first block after play start will schedule.
    void primeStandbyAtPlayStart();

    bool scheduleClip(const audium::DspClip &clip,
                      VoiceSource* voiceSource,
                      double transportPosition,
                      int sampleOffset,
                      int numSamples);
    
    void startPlaying();
    void stopPlaying();
    bool isPlaying() const;
    void setFollowTransport(bool enable) { data.followTransport = enable; }
    bool getFollowTransport() const { return data.followTransport; }
    
    [[deprecated]]
    void setEditMode(bool bEditMode) { data.editMode = bEditMode; }
    
    [[deprecated]]
    bool isEditMode() const { return data.editMode; }
    
    [[deprecated]]
    bool isArrangementMode() const { return !data.editMode; }
    
    void setCurrentPositionAtPlayListItemIndex(std::shared_ptr<AudioTrack> track, int playListItemIndex);
    int getPlayListItemIndexAtCurrentPosition(std::shared_ptr<AudioTrack> track);
    double getPlayListItemProgress(std::shared_ptr<AudioTrack> track, int playListItemIndex) const;
    
    void setAbsoluteStartPosition(double newPosition, audium::TimeContextType context);
    double getAbsoluteStartPosition(audium::TimeContextType context) const;
    double getAbsolutePosition(audium::TimeContextType context) const;
    
    template <typename ProcessContext>
    void process (const ProcessContext& context, bool isPlaying, double beats, int numSamples) noexcept
    {
        audioBusInterface->setNumAudioBusChannels(audioTrackContainer->getNumAudioTrackChannels());
        
        if (isPlaying &&
            beats >= 0.0) {
            
            auto timeContext = audium::seconds;
            auto pos = 0.0;
            if (timeContext == clocks)
                pos = tempoProvider->beatsToClocks(beats);
            else
                pos = tempoProvider->beatsToSeconds(beats);
            
            auto loopResult = transportLoop->processLoop(pos, numSamples, timeContext);
            
            if (loopResult.context == audium::seconds)
                data.transportPositionClocks = tempoProvider->secondsToClocks(loopResult.positionResult);
            else
                data.transportPositionClocks = loopResult.positionResult;
            
            process(data.transportPositionClocks, numSamples, loopResult);
            
        }
        
        audioBusInterface->process(context);
    }
        
    double getTotalLength(audium::TimeContextType context, bool addOverhead = false) const;
    
    void bouncePlayListItem(juce::AudioFormatWriter* writer,
                            std::shared_ptr<ExportAudioConfig> config,
                            std::function<void ()> callback);
    
    void bounceProject(juce::AudioFormatWriter* writer,
                      std::shared_ptr<ExportAudioConfig> config,
                      std::function<void ()> callback);
    
    std::shared_ptr<audium::LinkEngine> getLinkEngine() const { return linkEngine; }
    std::shared_ptr<TempoProvider> getTempoProvider() const { return tempoProvider; }
    std::shared_ptr<Playback> getPlayback() const { return playback; }
    std::shared_ptr<AudioBusInterface> getAudioBusInterface() const { return audioBusInterface; }
    std::shared_ptr<TransportLoop> getTransportLoop() const { return transportLoop; }
    std::shared_ptr<AudioTrackContainer> getAudioTrackContainer() const { return audioTrackContainer; } 
    
    std::vector<std::shared_ptr<PlayListItem>> getPlayListItems(bool excludeSelectedItems = true) const;
    
    void commitPlayListData();
    
    // recording
    bool anyTrackRecordEnabled() const;
    void startRecording(const int channelNumber = -1);
    void stopRecording(const int channelNumber = -1);
    bool isRecordingArmed() const noexcept { return data.isRecordingArmed; }
    void setRecordingArmed(bool bArmed) { data.isRecordingArmed = bArmed; }
    double getRecordingLength(audium::TimeContextType context) const;
    
    bool isRecording() const noexcept;
    
#if CATCH2_TESTS
    void recordFromAudioBuffer(const AudioBuffer<float> &inBuffer,
                               double recLengthSeconds);
#endif
    
    PlayListSchedulerData data;

    /// Stretched clips prime a standby stretcher over the blocks before
    /// their start (clip start, loop wrap, play start) instead of inside
    /// that block (see StretchAudioSource::primeStandby). Tests flip it
    /// off to compare.
    std::atomic<bool> standbyPrimingEnabled { true };

    /// How far ahead of a start the standby prime begins, in seconds.
    static constexpr double standbyPrimeHorizonSeconds = 0.25;
    
    std::function<void()> onRecordingStartedFunction;
    
private:
    
    // process sequencing
    void process(double absolutePosition,
                 int numSamples,
                 const TransportLoop::LoopResult loopResult);
    
    
    /// A fresh clip snapshot reached a playing voice. Only the gain moved:
    /// applied live. A RePitch clip: restarts now (cheap). A Stretch clip
    /// with a standby lane: keeps playing its old alignment until a position
    /// snapshotRestartDeferSeconds ahead while the lane primes for it, so
    /// the restart there only swaps lanes instead of priming in the block.
    void onClipSnapshotChanged(const audium::DspClip& dspClip,
                               VoiceSource* voiceSource,
                               const TransportLoop::LoopResult& loopResult,
                               double transportPosition,
                               int numSamples);
    /// Lands a deferred restart in the block holding its position: the
    /// voice re-seeks mid-block at exactly the primed position (like a loop
    /// wrap), so timing jitter between blocks cannot miss the standby key.
    void restartAtPendingPosition(const audium::DspClip& dspClip,
                                  VoiceSource* voiceSource,
                                  double transportPosition,
                                  int numSamples);
    /// How long a playing Stretch clip keeps its old alignment after its
    /// clip changed while the standby lane primes for the restart.
    static constexpr double snapshotRestartDeferSeconds = 0.05;

    std::shared_ptr<AudioTrackContainer> audioTrackContainer;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<TempoProvider> tempoProvider;
    std::shared_ptr<LinkEngine> linkEngine;
    std::shared_ptr<AudioClipContainer> audioClipContainer;
    std::shared_ptr<VoiceSourceContainer> voiceSourceContainer;
    std::shared_ptr<Playback> playback;
    std::shared_ptr<AudioBusInterface> audioBusInterface;
    std::shared_ptr<TransportLoop> transportLoop;
    
    double externalSampleRate = 0.0;
    
    int bufferSize = 0;
    
    std::atomic<bool> forcePosition = false;
    
    std::atomic<double> totalLengthClocks = 0.0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListScheduler)
};

} // namespace audium
