/*
  ==============================================================================

    AudiumEngine.cpp
    Created: 29 Jan 2023 12:31:48pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudiumEngine.h"

AudiumEngine::AudiumEngine(std::shared_ptr<AudioResourceContainer> container) :
    audioResourceContainer(container)
{
}

AudiumEngine::~AudiumEngine()
{
    
}
