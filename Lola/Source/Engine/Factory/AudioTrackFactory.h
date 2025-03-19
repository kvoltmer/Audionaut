//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/Group/AudioSubGroup.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Region/AudioRegionContainer.h"

namespace audium {

class AudioTrackFactory {
    
public:
    AudioTrackFactory() = default;
    
    static std::shared_ptr<AudioTrack> createAudioTrack(AudioTrackContainer &owner,
                                                        std::shared_ptr<AudioResourceContainer> audioResourceContainer)
    {
        auto subGroups  = std::shared_ptr<tAudioSubGroupContainer> (new tAudioSubGroupContainer());
        auto channels   = std::shared_ptr<tAudioChannelContainer> (new tAudioChannelContainer());
        auto audioTrack = std::shared_ptr<AudioTrack>(new AudioTrack(owner,
                                                                     *audioResourceContainer.get(),
                                                                     owner.getTransportSourceContainer(),
                                                                     owner.getSelectionManager(),
                                                                     subGroups,
                                                                     channels,
                                                                     std::string()));
        return audioTrack;
    }
    
    static std::shared_ptr<AudioSubGroup> createAudioSubGroup(AudioTrack &track)
    {
        auto regionContainer = std::shared_ptr<AudioRegionContainer> (new AudioRegionContainer(track));
        return std::shared_ptr<AudioSubGroup> (new AudioSubGroup(track,
                                                                 regionContainer,
                                                                 track.getSelectionManager()));
    }
};

} // namespace audium
