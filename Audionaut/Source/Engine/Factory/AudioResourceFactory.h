//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/AudioSources/AudiumTransportSource.h"

namespace audium {

/**
 * @class AudioResourceFactory
 * @brief Factory class for creating audio resources and associated objects.
 *
 * This class provides static methods to create instances of `AudioResource` and
 * `juce::AudioFormatReader`. It centralizes the creation logic to ensure consistency
 * and simplify object management.
 */
class AudioResourceFactory {
    
public:
    /**
     * @brief Default constructor for `AudioResourceFactory`.
     */
    AudioResourceFactory() = default;
    
    /**
     * @brief Creates a new audio resource.
     * @param url The URL of the audio file.
     * @param reader Shared pointer to the audio format reader.
     * @param audioResourceContainer Reference to the container holding audio resources.
     * @param track Shared pointer to the associated audio track.
     * @param resourceGroup Shared pointer to the associated resource group.
     * @param destChannel The destination channel index.
     * @param sourceChannel The source channel index.
     * @return A shared pointer to the created `AudioResource` instance.
     */
    static std::shared_ptr<AudioResource> createAudioResource(juce::URL url,
                                                              std::shared_ptr<juce::AudioFormatReader> reader,
                                                              AudioResourceContainer& audioResourceContainer,
                                                              std::shared_ptr<AudioTrack> track,
                                                              std::shared_ptr<ResourceGroup> resourceGroup,
                                                              int destChannel,
                                                              int sourceChannel)
    {
        return std::make_shared<AudioResource>(audioResourceContainer,
                                               track,
                                               resourceGroup,
                                               url,
                                               reader,
                                               destChannel,
                                               sourceChannel);
    }
    
    /**
     * @brief Creates a new audio format reader for a given URL.
     * @param url The URL of the audio file.
     * @param formatManager Reference to the audio format manager.
     * @return A shared pointer to the created `juce::AudioFormatReader` instance, or `nullptr` if creation fails.
     */
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

} // namespace audium
