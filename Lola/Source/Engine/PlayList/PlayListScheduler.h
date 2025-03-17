//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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

namespace audium {

class PlayListContainer;
class PlayListItem;
class TransportSourceContainer;
class AudioResourceContainer;
class Playback;
class AudioBusInterface;


class PlayListScheduler : public juce::ChangeListener
{
    
    
public:
    PlayListScheduler(std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
                      std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                      std::shared_ptr<TempoProvider> tempoProvider_,
                      std::shared_ptr<audium::LinkEngine> linkEngine_,
                      std::shared_ptr<audium::AudioClipContainer> audioClipContainer_,
                      std::shared_ptr<TransportSourceContainer> transportSourceContainer_,
                      std::shared_ptr<audium::Playback> playback_,
                      std::shared_ptr<AudioBusInterface> audioBusInterface_,
                      std::shared_ptr<TransportLoop> transportLoop_) :
    audioTrackContainer(audioTrackContainer_),
    audioResourceContainer(audioResourceContainer_),
    tempoProvider(tempoProvider_),
    linkEngine(linkEngine_),
    audioClipContainer(audioClipContainer_),
    transportSourceContainer(transportSourceContainer_),
    playback(playback_),
    audioBusInterface(audioBusInterface_),
    transportLoop(transportLoop_)
    {
        linkEngine->tickCallback = [this](bool isPlaying, double beats, int numSamples) {
            tick(isPlaying, beats, numSamples);
        };
        
        audioTrackContainer->addChangeListener(this);
    }
    
    ~PlayListScheduler() override
    {
        audioTrackContainer->removeChangeListener(this);
    }
    
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        commitPlayListData();
    }
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);
    
    
    void startPlaying();
    void stopPlaying();
    bool isPlaying() const;
    void setFollowTransport(bool enable) { data.followTransport = enable; }
    bool getFollowTransport() const { return data.followTransport; }
    void setEditMode(bool bEditMode) { data.editMode = bEditMode; }
    bool isEditMode() const { return data.editMode; }
    bool isArrangementMode() const { return !data.editMode; }
    
    void setCurrentPositionAtPlayListItemIndex(std::shared_ptr<AudioTrack> track, int playListItemIndex);
    int getPlayListItemIndexAtCurrentPosition(std::shared_ptr<AudioTrack> track);
    double getPlayListItemProgress(std::shared_ptr<AudioTrack> track, int playListItemIndex) const;
    
    void setAbsoluteStartPosition(double newPosition, audium::TimeContextType context);
    double getAbsoluteStartPosition(audium::TimeContextType context) const;
    double getAbsolutePosition(audium::TimeContextType context) const;
    
    
    void tick(bool isPlaying, double beats, int numSamples);
    
    void processAudio (const juce::AudioSourceChannelInfo& info);
    
    double getTotalLength(audium::TimeContextType context, bool addOverhead = false) const;
    
    void bounceToFile(juce::AudioFormatWriter* writer,
                      audium::ExportAudioConfig &config,
                      std::function<void ()> callback);
    
    std::shared_ptr<audium::LinkEngine> getLinkEngine() const { return linkEngine; }
    std::shared_ptr<TempoProvider> getTempoProvider() const { return tempoProvider; }
    std::shared_ptr<audium::Playback> getPlayback() const { return playback; }
    std::shared_ptr<AudioBusInterface> getAudioBusInterface() const { return audioBusInterface; }
    std::shared_ptr<TransportLoop> getTransportLoop() const { return transportLoop; }
    
    void commitPlayListData();
    
    PlayListSchedulerData data;
    
private:
    
    // process sequencing
    void process(double absolutePosition, int numSamples, bool onLoop);
    
    
    std::shared_ptr<AudioTrackContainer> audioTrackContainer;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<TempoProvider> tempoProvider;
    std::shared_ptr<audium::LinkEngine> linkEngine;
    std::shared_ptr<audium::AudioClipContainer> audioClipContainer;
    std::shared_ptr<TransportSourceContainer> transportSourceContainer;
    std::shared_ptr<audium::Playback> playback;
    std::shared_ptr<AudioBusInterface> audioBusInterface;
    std::shared_ptr<TransportLoop> transportLoop;
    
    double externalSampleRate = 0.0;
    
    int bufferSize = 0;
    
    std::atomic<bool> forcePosition = false;
    
    std::atomic<double> totalLengthClocks = 0.0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListScheduler)
};

} // namespace audium
