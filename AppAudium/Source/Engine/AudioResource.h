/*
  ==============================================================================

    AudioResource.h
    Created: 29 Jan 2023 12:55:52pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <memory>

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>


class AudioResourceContainer;
class AudioPlayer;
class AudiumTransportSource;

class AudioResource {
    
public:
    AudioResource(AudioResourceContainer& audioResourceContainer,
                  juce::URL url,
                  std::shared_ptr<AudiumTransportSource> transportSource,
                  std::unique_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource,
                  std::shared_ptr<juce::AudioThumbnail> audioThumbnail) :
        owner(audioResourceContainer),
        url(url),
        transportSource(transportSource),
        audioThumbnail(audioThumbnail)
    {
        this->audioFormatReaderSource = std::move(audioFormatReaderSource);
        
        
//        audioThumbnail = std::unique_ptr<juce::AudioThumbnail>(new juce::AudioThumbnail(4096*4,
//                                                                                                 *formatManager.get(),
//                                                                                                 *audioThumbnailCache.get()));
//            //std::cout << "new thumbnail " << audioThumbnail.get() << " for resource " << resource.get() << std::endl;
//            if (auto inputSource = std::unique_ptr<juce::URLInputSource> (new juce::URLInputSource(resource->getUrl())))
//            {
//                if (auto stream = rawToUniquePtr (inputSource->createInputStream()))
//                {
//                    if (auto reader = rawToUniquePtr (formatManager->createReaderFor (std::move (stream))))
//                    {
//        //                    auto hashsource = name + resource->getUrl().getLocalFile().getFullPathName();
//        //                    audioThumbnail->setReader(reader.release(), hashsource.hash());
//
//                        audioThumbnail->setReader(reader.release(), inputSource->hashCode());
//                    }
//                }
//            }
        
    }
    
    ~AudioResource();

    std::shared_ptr<AudiumTransportSource> getAudioTransportSource() const { return transportSource; }

    const juce::String getFileNameWithoutExtension() const;
    
    const juce::String getFullPathName() const;
    
    const juce::URL getUrl() const { return url; }
    
    // Returns a string version of the URL.
    const juce::String getUrlAsString() const;
    
    /// TODO: move this to AudioGroupListBoxModel
    int getHeight() const { return height * getNumChannels(); }
    int getChannelHeight() const { return height; }
    
    AudioResourceContainer& getContainer() const { return owner; }
    
    double getSampleRate() const;
    unsigned int getNumChannels() const;
    double getLengthInSeconds() const;
    
    juce::AudioThumbnail* getAudioThumbnail() const { return audioThumbnail.get(); }
    
private:

    AudioResourceContainer& owner;
    
    juce::URL url;
    
    std::shared_ptr<AudiumTransportSource> transportSource;
    
    std::unique_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource;
    
    std::shared_ptr<juce::AudioThumbnail> audioThumbnail;
    //std::shared_ptr<juce::AudioThumbnailCache> audioThumbnailCache;
    
    int height = 100;
    
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResource)
};
