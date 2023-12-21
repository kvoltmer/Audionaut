/*
  ==============================================================================

    AudioGroupFactory.h
    Created: 8 Nov 2023 4:34:05pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "Engine/TransportSourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/Group/AudioSubGroup.h"

class AudioGroupFactory {
    
public:
    AudioGroupFactory() = default;
    
    static std::shared_ptr<AudioGroup> createAudioGroup(const AudioResourceContainer &audioResourceContainer,
                                                        const AudioRegionContainer &audioRegionContainer)
    {
        auto transportSourceContainer   = std::shared_ptr<TransportSourceContainer> (new TransportSourceContainer());
        auto playListContainer = std::shared_ptr<PlayListContainer> (new PlayListContainer(audioRegionContainer));
        auto audioGroup = std::shared_ptr<AudioGroup>(new AudioGroup(audioResourceContainer,
                                                                     audioRegionContainer,
                                                                     playListContainer,
                                                                     transportSourceContainer,
                                                                     std::string(), -1));
        return audioGroup;
    }
    
    static std::shared_ptr<AudioSubGroup> createAudioSubGroup(const AudioResourceContainer &audioResourceContainer,
                                                              const AudioRegionContainer &audioRegionContainer,
                                                              const AudioGroup &audioGroup)
    {
        return std::shared_ptr<AudioSubGroup>(new AudioSubGroup(audioGroup, -1));
    }
};
