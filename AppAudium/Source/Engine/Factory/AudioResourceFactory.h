/*
  ==============================================================================

    AudiumFactory.h
    Created: 27 Jun 2023 10:41:00am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <memory>
#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/AudioResource.h"
#include "Engine/AudioGroup.h"
#include "Engine/TransportSourceContainer.h"
#include "Engine/AudiumTransportSource.h"

class AudioResourceFactory {
    
public:
    AudioResourceFactory() = default;
    
    static std::shared_ptr<AudioResource> createAudioResource(juce::URL url,
                                                              AudioResourceContainer& audioResourceContainer,
                                                              std::shared_ptr<AudioGroup> group,
                                                              juce::AudioFormatManager& formatManager,
                                                              juce::TimeSliceThread* readAheadThread,
                                                              int channelPosition,
                                                              double transportPosition,
                                                              int resourceId)
    {
        std::shared_ptr<AudioResource> audioResource = nullptr;
        
        if (auto inputSource = makeAudioInputSource (url))
        {
            if (auto stream = rawToUniquePtr (inputSource->createInputStream()))
            {
                if (auto reader = rawToUniquePtr (formatManager.createReaderFor (std::move (stream))))
                {
                    auto audioFormatReaderSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
                    
                    auto transportSource = group->getTransportSourceContainer()->createNewTransportSource();
                    transportSource->setSource (audioFormatReaderSource.get(),
                                                32768,                   // tells it to buffer this many samples ahead
                                                readAheadThread,         // this is the background thread to use for reading-ahead
                                                audioFormatReaderSource->getAudioFormatReader()->sampleRate);     // allows for sample rate correction
                    
                    
                    auto audioThumbnail = std::shared_ptr<juce::AudioThumbnail>(new juce::AudioThumbnail(4096*4,
                                                                                                             formatManager,
                                                                                                             *audioResourceContainer.getAudioThumbnailCache().get()));
                    audioThumbnail->setSource(inputSource.release());
                    
                    audioResource = std::shared_ptr<AudioResource>(new AudioResource(audioResourceContainer,
                                                                                     group,
                                                                                     url,
                                                                                     transportSource,
                                                                                     std::move(audioFormatReaderSource),
                                                                                     audioThumbnail,
                                                                                     channelPosition,
                                                                                     resourceId));
                    audioResource->setTransportPosition(transportPosition, false);
                }
            }
        }
        jassert(audioResource != nullptr);
        return audioResource;
    }
    
    static std::unique_ptr<juce::InputSource> makeAudioInputSource (const juce::URL& url)
    {
       #if JUCE_ANDROID
        if (auto doc = AndroidDocument::fromDocument (url))
            return std::make_unique<AndroidDocumentInputSource> (doc);
       #endif

       #if ! JUCE_IOS
        if (url.isLocalFile())
            return std::make_unique<juce::FileInputSource> (url.getLocalFile());
       #endif

        return std::make_unique<juce::URLInputSource> (url);
    }
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResourceFactory)
};
