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
#include "Engine/Group/AudioGroup.h"
#include "Engine/Region/AudioRegionContainer.h"

class AudioGroupFactory {
    
public:
    AudioGroupFactory() = default;
    
    static std::shared_ptr<AudioGroup> createAudioGroup(AudioGroupContainer &owner,
                                                        AudioResourceContainer &audioResourceContainer)
    {
        auto transportSourceContainer   = std::shared_ptr<TransportSourceContainer> (new TransportSourceContainer());
        
        auto audioRegionContainer       = std::shared_ptr<AudioRegionContainer>     (new AudioRegionContainer(audioResourceContainer,
                                                                                                              owner,
                                                                                                              owner.getTempoProvider(),
                                                                                                              owner.getUndoManager()));
        
        auto playListContainer = std::shared_ptr<PlayListContainer> (new PlayListContainer(*audioRegionContainer.get(),
                                                                                           owner.getTempoProvider()));
        

        auto audioGroup = std::shared_ptr<AudioGroup>(new AudioGroup(owner,
                                                                     audioResourceContainer,
                                                                     audioRegionContainer,
                                                                     playListContainer,
                                                                     transportSourceContainer,
                                                                     std::string()));
        return audioGroup;
    }
    
    static std::shared_ptr<AudioSubGroup> createAudioSubGroup(AudioGroup &audioGroup)
    {
        return std::shared_ptr<AudioSubGroup>(new AudioSubGroup(audioGroup));
    }
};
