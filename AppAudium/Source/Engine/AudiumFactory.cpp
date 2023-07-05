/*
  ==============================================================================

    AudiumFactory.cpp
    Created: 27 Jun 2023 10:41:00am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudiumFactory.h"
#include "Engine/TransportSourceContainer.h"

std::shared_ptr<AudiumEngine> AudiumFactory::createAudiumEngine()
{
    auto transportSourceContainer   = std::shared_ptr<TransportSourceContainer> (new TransportSourceContainer());
    auto audioResourceContainer     = std::shared_ptr<AudioResourceContainer>   (new AudioResourceContainer(transportSourceContainer));
    auto audioRegionContainer       = std::shared_ptr<AudioRegionContainer>     (new AudioRegionContainer());
    auto playListContainer          = std::shared_ptr<PlayListContainer>        (new PlayListContainer(audioRegionContainer));
    auto audiumEngine               = std::shared_ptr<AudiumEngine>             (new AudiumEngine(audioResourceContainer,
                                                                                                  audioRegionContainer,
                                                                                                  playListContainer,
                                                                                                  transportSourceContainer));
    
    // this should happen outside of the factory
    audioResourceContainer->initializeAudioDevice();

    return audiumEngine;
}

