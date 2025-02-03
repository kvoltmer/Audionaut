/*
  ==============================================================================

    AudioTrackFactory.h
    Created: 8 Nov 2023 4:34:05pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/Group/AudioSubGroup.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Region/AudioRegionContainer.h"

class AudioTrackFactory {
    
public:
    AudioTrackFactory() = default;
    
    static std::shared_ptr<AudioTrack> createAudioTrack(AudioTrackContainer &owner,
                                                        std::shared_ptr<AudioResourceContainer> audioResourceContainer)
    {
        
        auto audioRegionContainer       = std::shared_ptr<AudioRegionContainer>     (new AudioRegionContainer(*audioResourceContainer.get(),
                                                                                                              owner,
                                                                                                              owner.getTempoProvider(),
                                                                                                              owner.getUndoManager()));
        
        auto playListContainer = std::shared_ptr<PlayListContainer> (new PlayListContainer(*audioRegionContainer.get(),
                                                                                           owner.getTempoProvider(),
                                                                                           owner.getTransportSourceContainer(),
                                                                                           owner.getSelectionManager()));
        
        auto subGroups  = std::shared_ptr<tAudioSubGroupContainer> (new tAudioSubGroupContainer());
        auto channels   = std::shared_ptr<tAudioChannelContainer> (new tAudioChannelContainer());
        auto audioTrack = std::shared_ptr<AudioTrack>(new AudioTrack(owner,
                                                                     *audioResourceContainer.get(),
                                                                     audioRegionContainer,
                                                                     playListContainer,
                                                                     owner.getTransportSourceContainer(),
                                                                     owner.getSelectionManager(),
                                                                     subGroups,
                                                                     channels,
                                                                     std::string()));
        return audioTrack;
    }
    
    static std::shared_ptr<AudioSubGroup> createAudioSubGroup(AudioTrack &audioTrack)
    {
        return std::shared_ptr<AudioSubGroup>(new AudioSubGroup(audioTrack, audioTrack.getSelectionManager()));
    }
};
