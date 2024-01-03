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
#include "Engine/Group/AudioGroup.h"
#include "Engine/TransportSourceContainer.h"
#include "Engine/AudiumTransportSource.h"

class AudioResourceFactory {
    
public:
    AudioResourceFactory() = default;
    
    static std::shared_ptr<AudioResource> createAudioResource(juce::URL url,
                                                              AudioResourceContainer& audioResourceContainer,
                                                              std::shared_ptr<AudioGroup> group,
                                                              std::shared_ptr<AudioSubGroup> subGroup,
                                                              juce::AudioFormatManager& formatManager,
                                                              juce::TimeSliceThread* readAheadThread,
                                                              int channelPosition,
                                                              double transportPosition,
                                                              int resourceId)
    {
        std::shared_ptr<AudioResource> audioResource = nullptr;
        auto readAheadBufferSize = 48000;
     
        if (auto inputSource = makeAudioInputSource (url))
        {
            if (auto stream = rawToUniquePtr (inputSource->createInputStream()))
            {
                std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource = nullptr;
                
                auto audioFormat = formatManager.findFormatForFileExtension(url.getLocalFile().getFileExtension());
                if (audioFormat != nullptr)
                {
                    if (auto memMappedReader = rawToUniquePtr(audioFormat->createMemoryMappedReader(url.getLocalFile())))
                    {
                        if (memMappedReader->mapEntireFile())
                        {
                            readAheadThread = nullptr;
                            readAheadBufferSize = 0;
                        }
                        audioFormatReaderSource = std::shared_ptr<juce::AudioFormatReaderSource> (new juce::AudioFormatReaderSource(memMappedReader.release(), true));
                    }
                    else if (auto reader = rawToUniquePtr (formatManager.createReaderFor (std::move (stream))))
                    {
                        audioFormatReaderSource = std::shared_ptr<juce::AudioFormatReaderSource> (new juce::AudioFormatReaderSource(reader.release(), true));
                        
                    }
                    jassert(audioFormatReaderSource);
                    
                    if (audioFormatReaderSource)
                    {
                        auto transportSource = group->getTransportSourceContainer()->createNewTransportSource();
                        transportSource->setSource (audioFormatReaderSource.get(),
                                                    readAheadBufferSize,
                                                    readAheadThread,
                                                    audioFormatReaderSource->getAudioFormatReader()->sampleRate);
                        
                        audioResource = std::shared_ptr<AudioResource>(new AudioResource(audioResourceContainer,
                                                                                         group,
                                                                                         subGroup,
                                                                                         url,
                                                                                         transportSource,
                                                                                         std::move(audioFormatReaderSource),
                                                                                         channelPosition,
                                                                                         resourceId));
                        audioResource->setTransportPosition(transportPosition, audium::seconds);
                    }
                    
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
