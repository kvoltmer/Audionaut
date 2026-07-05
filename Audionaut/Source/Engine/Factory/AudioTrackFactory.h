//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/Group/ResourceGroup.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Analysis/AnalysisProvider.h"

namespace audium {

/**
 * @class AudioTrackFactory
 * @brief Factory class for creating audio tracks and associated resource groups.
 *
 * This class provides static methods to create instances of `AudioTrack` and `ResourceGroup`.
 * It centralizes the creation logic to ensure consistency and simplify object management.
 */
class AudioTrackFactory {
    
public:
    /**
     * @brief Default constructor for `AudioTrackFactory`.
     */
    AudioTrackFactory() = default;
    
    /**
     * @brief Creates a new audio track.
     * @param owner Reference to the `AudioTrackContainer` that owns the track.
     * @param audioResourceContainer Shared pointer to the container holding audio resources.
     * @return A shared pointer to the created `AudioTrack` instance.
     */
    static std::shared_ptr<AudioTrack> createAudioTrack(AudioTrackContainer &owner,
                                                        std::shared_ptr<AudioResourceContainer> audioResourceContainer)
    {
        auto resourceGroups  = std::shared_ptr<tResourceGroupContainer> (new tResourceGroupContainer());
        auto channels   = std::shared_ptr<tAudioChannelContainer> (new tAudioChannelContainer());
        auto sBicSegmenter = std::make_shared<SBicSegmenter>();
        auto onsetSegmenter = std::make_shared<OnsetSegmenter>();
        auto analysisCache = std::make_shared<AnalysisCache>();
        auto analysisProvider = std::shared_ptr<AnalysisProvider>(new AnalysisProvider(owner, sBicSegmenter, onsetSegmenter, analysisCache));
        auto audioTrack = std::shared_ptr<AudioTrack>(new AudioTrack(owner,
                                                                     *audioResourceContainer.get(),
                                                                     owner.getTransportSourceContainer(),
                                                                     owner.getSelectionManager(),
                                                                     resourceGroups,
                                                                     channels,
                                                                     analysisProvider,
                                                                     std::string()));
        return audioTrack;
    }
    
    /**
     * @brief Creates a new resource group for an audio track.
     * @param track Reference to the `AudioTrack` for which the resource group is created.
     * @return A shared pointer to the created `ResourceGroup` instance.
     */
    static std::shared_ptr<ResourceGroup> createResourceGroup(AudioTrack &track)
    {
        auto regionContainer = std::shared_ptr<AudioRegionContainer> (new AudioRegionContainer(track));
        return std::shared_ptr<ResourceGroup> (new ResourceGroup(track,
                                                                 regionContainer,
                                                                 track.getSelectionManager()));
    }
};

} // namespace audium
