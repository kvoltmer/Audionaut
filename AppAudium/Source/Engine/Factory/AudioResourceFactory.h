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
                                                              int resourceId)
    {
        std::shared_ptr<AudioResource> audioResource = nullptr;
     
        if (auto inputSource = makeAudioInputSource (url))
        {
            if (auto stream = rawToUniquePtr (inputSource->createInputStream()))
            {
                auto audioFormat = formatManager.findFormatForFileExtension(url.getLocalFile().getFileExtension());
                if (audioFormat != nullptr)
                {
                    AudioFormatReader* reader = audioFormat->createMemoryMappedReader(url.getLocalFile());
                    auto readAheadBufferSize = 48000;
                    if (reader == nullptr)
                    {
                        reader = audioFormat->createReaderFor(stream.release(), false);
                    }
                    else
                    {
                        auto memReader = dynamic_cast<MemoryMappedAudioFormatReader*>(reader);
                        if (memReader->mapEntireFile())
                        {
                            readAheadThread = nullptr;
                            readAheadBufferSize = 0;
                        }
                    }
                    
                    if (reader != nullptr)
                    {
                        auto audioFormatReaderSource    = std::shared_ptr<juce::AudioFormatReaderSource> (new juce::AudioFormatReaderSource(reader, true));
                        auto transportSource            = group->getTransportSourceContainer()->createNewTransportSource();
                        transportSource->setSource (audioFormatReaderSource.get(),
                                                    readAheadBufferSize,
                                                    readAheadThread,
                                                    reader->sampleRate);
                        
                        audioResource = std::shared_ptr<AudioResource>(new AudioResource(audioResourceContainer,
                                                                                         group,
                                                                                         subGroup,
                                                                                         url,
                                                                                         transportSource,
                                                                                         audioFormatReaderSource,
                                                                                         channelPosition,
                                                                                         resourceId));
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
