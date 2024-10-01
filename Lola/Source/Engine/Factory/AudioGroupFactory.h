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
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/Region/AudioRegionContainer.h"

class AudioGroupFactory {
    
public:
    AudioGroupFactory() = default;
    
    static std::shared_ptr<AudioGroup> createAudioGroup(AudioGroupContainer &owner,
                                                        std::shared_ptr<AudioResourceContainer> audioResourceContainer)
    {
        
        auto audioRegionContainer       = std::shared_ptr<AudioRegionContainer>     (new AudioRegionContainer(*audioResourceContainer.get(),
                                                                                                              owner,
                                                                                                              owner.getTempoProvider(),
                                                                                                              owner.getUndoManager()));
        
        auto playListContainer = std::shared_ptr<PlayListContainer> (new PlayListContainer(*audioRegionContainer.get(),
                                                                                           owner.getTempoProvider(),
                                                                                           owner.getTransportSourceContainer()));
        

        auto audioGroup = std::shared_ptr<AudioGroup>(new AudioGroup(owner,
                                                                     *audioResourceContainer.get(),
                                                                     audioRegionContainer,
                                                                     playListContainer,
                                                                     owner.getTransportSourceContainer(),
                                                                     owner.getSelectionManager(),
                                                                     std::string()));
        return audioGroup;
    }
    
    static std::shared_ptr<AudioSubGroup> createAudioSubGroup(AudioGroup &audioGroup)
    {
        return std::shared_ptr<AudioSubGroup>(new AudioSubGroup(audioGroup));
    }
};
