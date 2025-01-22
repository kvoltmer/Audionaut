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
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/AudioSources/AudiumTransportSource.h"

class AudioResourceFactory {
    
public:
    AudioResourceFactory() = default;
    
    static std::shared_ptr<AudioResource> createAudioResource(juce::URL url,
                                                              AudioResourceContainer& audioResourceContainer,
                                                              std::shared_ptr<AudioTrack> track,
                                                              std::shared_ptr<AudioSubGroup> subGroup,
                                                              int channelPosition,
                                                              std::shared_ptr<juce::AudioFormatReader> reader)
    {
        return std::make_shared<AudioResource>(audioResourceContainer,
                                               track,
                                               subGroup,
                                               url,
                                               channelPosition,
                                               reader);
    }
    
    static std::shared_ptr<juce::AudioFormatReader> createAudioFormatReader(juce::URL url,
                                                                      juce::AudioFormatManager& formatManager)
    {
        if (auto audioFormat = formatManager.findFormatForFileExtension(url.getLocalFile().getFileExtension())) {
            
            auto reader = std::shared_ptr<AudioFormatReader>(audioFormat->createMemoryMappedReader(url.getLocalFile()));
            if (reader != nullptr) {
                if (auto memReader = dynamic_cast<juce::MemoryMappedAudioFormatReader*>(reader.get())) {
                    memReader->mapEntireFile();
                }
                return reader;
            }
            else if (auto inputSource = std::make_unique<juce::URLInputSource>(url)) {
                if (auto stream = rawToUniquePtr (inputSource->createInputStream())) {
                    return std::shared_ptr<juce::AudioFormatReader>(audioFormat->createReaderFor(stream.release(), false));
                }
            }
        }
        return nullptr;
    }
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResourceFactory)
};
