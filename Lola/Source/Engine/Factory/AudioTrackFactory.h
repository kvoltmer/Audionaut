//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
