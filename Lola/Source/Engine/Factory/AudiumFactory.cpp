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

#include "AudiumFactory.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/Link/LinkEngine.hpp"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Core/AudioClipContainer.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/Playback/Playback.h"
#include "Engine/Playback/AudioBusRenderer.h"
#include "Engine/Core/LockFreeCommander.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/PlayList/TransportLoop.h"

using namespace audium;

std::shared_ptr<AudiumEngine> AudiumFactory::createAudiumEngine()
{
    auto undoManager                = std::make_shared<juce::UndoManager>();
    
    auto selectionManager           = std::make_shared<audium::SelectionManager>();
    
    auto audioDeviceManager         = std::make_shared<juce::AudioDeviceManager>();
    
    auto linkEngine                 = std::make_shared<LinkEngine>();
    
    auto tempoProvider              = std::make_shared<TempoProvider>(linkEngine);
    
    auto formatManager              = std::make_shared<juce::AudioFormatManager>();
    
    auto audioThumbnailCache        = std::make_shared<juce::AudioThumbnailCache>(64);
    
    auto playback                   = std::make_shared<audium::Playback>();
    
    auto audioBusRenderer           = std::make_shared<AudioBusRenderer<float>>(playback);
    
    auto transportSourceContainer   = std::make_shared<TransportSourceContainer>(playback);
    
    auto audioResourceContainer     = std::make_shared<AudioResourceContainer>(audioDeviceManager,
                                                                               formatManager,
                                                                               audioThumbnailCache,
                                                                               tempoProvider,
                                                                               transportSourceContainer);
    
    auto lockFreeCommander          = std::make_shared<LockFreeCommander>(256);
    
    auto audioBusInterface          = std::make_shared<AudioBusInterface>(lockFreeCommander, audioBusRenderer);
    
    auto transportLoop              = std::make_shared<TransportLoop>(undoManager,
                                                                      tempoProvider);
    
    auto audioTrackContainer        = std::make_shared<AudioTrackContainer>(undoManager,
                                                                            tempoProvider,
                                                                            audioResourceContainer,
                                                                            transportSourceContainer,
                                                                            selectionManager,
                                                                            audioBusInterface,
                                                                            transportLoop);
    
    auto audioClipContainer         = std::make_shared<AudioClipContainer>(1024);
    
    
    
    auto playListScheduler          = std::make_shared<PlayListScheduler>(audioTrackContainer,
                                                                          audioResourceContainer,
                                                                          tempoProvider,
                                                                          linkEngine,
                                                                          audioClipContainer,
                                                                          transportSourceContainer,
                                                                          playback,
                                                                          audioBusInterface,
                                                                          transportLoop);
    
    auto linkAudioDevice            = std::make_shared<LinkAudioDevice>(linkEngine,
                                                                        playListScheduler);
        
    auto audiumEngine               = std::make_shared<AudiumEngine>(audioDeviceManager,
                                                                     audioTrackContainer,
                                                                     audioResourceContainer,
                                                                     playListScheduler,
                                                                     linkAudioDevice,
                                                                     undoManager,
                                                                     audioBusInterface);
    return audiumEngine;
}

