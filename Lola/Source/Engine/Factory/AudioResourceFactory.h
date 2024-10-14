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
#include "Engine/Resource/AudioResource.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/TransportSourceContainer.h"
#include "Engine/AudiumTransportSource.h"

class AudioResourceFactory {
    
public:
    AudioResourceFactory() = default;
    
    static std::shared_ptr<AudioResource> createAudioResource(juce::URL url,
                                                              AudioResourceContainer& audioResourceContainer,
                                                              std::shared_ptr<AudioTrack> track,
                                                              std::shared_ptr<AudioSubGroup> subGroup,
                                                              int channelPosition)
    {
        return std::shared_ptr<AudioResource>(new AudioResource(audioResourceContainer,
                                                                         track,
                                                                         subGroup,
                                                                         url,
                                                                         channelPosition));
    }
    
    static std::shared_ptr<juce::AudioFormatReaderSource> createAudioFormatReaderSource(juce::URL url,
                                                                                        juce::AudioFormatManager& formatManager)
    {
        std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource = nullptr;
     
        if (auto inputSource = makeAudioInputSource (url))
        {
            if (auto stream = rawToUniquePtr (inputSource->createInputStream()))
            {
                auto audioFormat = formatManager.findFormatForFileExtension(url.getLocalFile().getFileExtension());
                if (audioFormat != nullptr)
                {
                    AudioFormatReader* reader = audioFormat->createMemoryMappedReader(url.getLocalFile());
                    if (reader == nullptr)
                    {
                        reader = audioFormat->createReaderFor(stream.release(), false);
                    }
                    else
                    {
                        auto memReader = dynamic_cast<MemoryMappedAudioFormatReader*>(reader);
                        if (memReader != nullptr)
                        {
                            memReader->mapEntireFile();
                        }
                    }
                    
                    if (reader != nullptr)
                    {
                        audioFormatReaderSource = std::shared_ptr<juce::AudioFormatReaderSource> (new juce::AudioFormatReaderSource(reader, true));
                    }
                }
            }
        }
        
        return audioFormatReaderSource;
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
