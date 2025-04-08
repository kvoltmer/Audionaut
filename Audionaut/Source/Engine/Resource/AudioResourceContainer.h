//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <vector>
#include <memory>
#include <JuceHeader.h>

#include "Engine/Resource/AudioResource.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioSubGroup.h"
#include "Engine/Provider/TempoProvider.h"

namespace audium {

class TransportSourceContainer;
class AudiumEngine;

class AudioResourceContainer :  public juce::ActionBroadcaster
{
public:
    AudioResourceContainer(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager_,
                           std::shared_ptr<juce::AudioFormatManager> formatManager_,
                           std::shared_ptr<juce::AudioThumbnailCache> audioThumbnailCache_,
                           std::shared_ptr<TempoProvider> tempoProvider_,
                           std::shared_ptr<TransportSourceContainer> transportSourceContainer_) :
    audioDeviceManager(audioDeviceManager_),
    formatManager(formatManager_),
    audioThumbnailCache(audioThumbnailCache_),
    tempoProvider(tempoProvider_),
    transportSourceContainer(transportSourceContainer_)
    {
        formatManager->registerBasicFormats();
        thread.startThread();
    }
    
    ~AudioResourceContainer();
    
    static bool createTemporaryProjectDirectory(bool reset);
    
    // returns the directory of the audio file directory
    // example: Foo.audium/Media/Audio
    static const juce::File getAudioFileDirectory(const juce::File projectRoot);
    
    static const juce::File getAudioFileDirectory();
    
    // copy or move audio files
    static void copyOrMoveAudioFiles(const juce::File sourceDirectory, const juce::File destinationDirectory);
    
    void changeAudioFilePaths(const juce::File newPath);
    
    // returns the url of the newly copied audio file or the input url in case no copy is needed
    static const juce::URL copyToAudioFileDirectoryIfNeeded(juce::URL url);
    
    std::shared_ptr<AudioResource> findResourceWithUrl(juce::URL url) const;
    
    // returns a AudioFormatReader. Note: this method may change the url
    std::shared_ptr<juce::AudioFormatReader> getAudioFormatReaderForUrl(juce::URL &url);
    std::shared_ptr<AudioResource> addAudioResource (juce::URL url,
                                                     std::shared_ptr<juce::AudioFormatReader> audioFormatReader,
                                                     std::shared_ptr<AudioTrack> track,
                                                     std::shared_ptr<AudioSubGroup> subGroup,
                                                     int destChannel = -1,
                                                     int sourceChannel = -1);
    
    std::shared_ptr<AudiumTransportSource> createTransportSourceForAudioResource(std::shared_ptr<AudioResource> audioResource);
    
    
    void removeAudioResource(std::shared_ptr<AudioResource> resource);
    void removeAudioResourcesForTrack (AudioTrack *track);
    
    // still used by auto edit
    int getNumAudioResources() const;
    
    std::vector<std::shared_ptr<AudioResource>> resourcesAtAbsolutePosition(double positionInSeconds) const;
    
    void cleanup();
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesForTrack(AudioTrack *track) const;
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesForSubGroup(const AudioSubGroup *subGroup) const;
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesForTrackAtAbsoluteRange(AudioTrack *track, juce::Range<double> rangeInSeconds) const;
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesForChannel(const AudioChannel *channel) const;
    
    std::shared_ptr<AudioTrack> getAudioTrackForResource(std::shared_ptr<AudioResource> resource) const;
    
    std::vector<std::shared_ptr<AudioTrack>> getAudioTracks() const;
    
    void onDeleteChannel(AudioTrack* audioTrack, AudioChannel* channel);
    
    std::shared_ptr<juce::AudioFormatManager> getAudioFormatManager() const { return formatManager; }
    std::shared_ptr<juce::AudioThumbnailCache> getAudioThumbnailCache() const { return audioThumbnailCache; }
    std::shared_ptr<TempoProvider> getTempoProvider() const { return tempoProvider; }
    std::shared_ptr<juce::AudioDeviceManager> getAudioDeviceManager() const { return audioDeviceManager; }
    
    typedef std::pair<std::shared_ptr<AudioTrack>, std::shared_ptr<AudioResource>> tAudioTrackPair;
    
    juce::TimeSliceThread *getReadAheadThread() { return &thread; }
    
private:
    /// list of pairs. this enables sorting etc.. by AudioTrack
    std::list<tAudioTrackPair> audioResources;
    
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;
    std::shared_ptr<juce::AudioFormatManager> formatManager;
    std::shared_ptr<juce::AudioThumbnailCache> audioThumbnailCache;
    std::shared_ptr<TempoProvider> tempoProvider;
    std::shared_ptr<TransportSourceContainer> transportSourceContainer;
    
    
    juce::TimeSliceThread thread  { "read ahead thread" };
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResourceContainer)
};

} // namespace audium
