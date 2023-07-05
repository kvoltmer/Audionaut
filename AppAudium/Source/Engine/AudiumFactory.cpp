/*
  ==============================================================================

    AudiumFactory.cpp
    Created: 27 Jun 2023 10:41:00am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudiumFactory.h"
#include "Engine/TransportSourceProvider.h"

std::shared_ptr<AudiumEngine> AudiumFactory::createAudiumEngine()
{
    auto transportSourceProvider   = std::shared_ptr<TransportSourceProvider> (new TransportSourceProvider());
    auto audioResourceContainer     = std::shared_ptr<AudioResourceContainer>   (new AudioResourceContainer(transportSourceProvider));
    auto audioRegionContainer       = std::shared_ptr<AudioRegionContainer>     (new AudioRegionContainer());
    auto playListContainer          = std::shared_ptr<PlayListContainer>        (new PlayListContainer(audioRegionContainer));
    auto audiumEngine               = std::shared_ptr<AudiumEngine>             (new AudiumEngine(audioResourceContainer,
                                                                                                  audioRegionContainer,
                                                                                                  playListContainer,
                                                                                                  transportSourceProvider));
    
    // this should happen outside of the factory
    audioResourceContainer->initializeAudioDevice();

    return audiumEngine;
}

