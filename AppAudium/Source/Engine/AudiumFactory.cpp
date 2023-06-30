/*
  ==============================================================================

    AudiumFactory.cpp
    Created: 27 Jun 2023 10:41:00am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudiumFactory.h"
#include "Engine/AudiumTransportSource.h"

std::shared_ptr<AudiumEngine> AudiumFactory::createAudiumEngine()
{
    auto audioResourceContainer = std::shared_ptr<AudioResourceContainer>   (new AudioResourceContainer());
    auto audioRegionContainer   = std::shared_ptr<AudioRegionContainer>     (new AudioRegionContainer());
    auto audiumTransportSource  = std::shared_ptr<AudiumTransportSource>    (new AudiumTransportSource(audioResourceContainer));
    auto playListContainer      = std::shared_ptr<PlayListContainer>        (new PlayListContainer(audioRegionContainer));
    auto audiumEngine           = std::shared_ptr<AudiumEngine>             (new AudiumEngine(audioResourceContainer,
                                                                                              audioRegionContainer,
                                                                                              playListContainer,
                                                                                              audiumTransportSource));
    
    // this should happen outside of the factory
    audioResourceContainer->initializeAudioDevice();

    return audiumEngine;
}

