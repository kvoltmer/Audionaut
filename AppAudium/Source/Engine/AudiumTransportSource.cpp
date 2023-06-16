/*
  ==============================================================================

    AudiumTransportSource.cpp
    Created: 14 Jun 2023 6:12:34pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudiumTransportSource.h"
#include "AudioResourceContainer.h"

void AudiumTransportSource::setPosition (double newPosition)
{
    /// TODO: this is a workaround
    for (int i = 0; i < audioResourceContainer->getAudioResourceSize(); i++)
    {
        audioResourceContainer->getAudioResource(i)->getAudioTransportSource()->setPosition(newPosition);
    }
    
    std::cout << "setPosition " << newPosition << std::endl;
}

double AudiumTransportSource::getCurrentPosition() const
{
    /// TODO: this is a workaround
    if (audioResourceContainer->getAudioResource(0) != nullptr)
    {
        return audioResourceContainer->getAudioResource(0)->getAudioTransportSource()->getCurrentPosition();
    }
    return 0;
}

void AudiumTransportSource::start()
{
    /// TODO: this is not thread save
    audioResourceContainer->start();
}


void AudiumTransportSource::stop()
{
    /// TODO: this is not thread save
    audioResourceContainer->stop();
}

bool AudiumTransportSource::isPlaying() const
{
    if (audioResourceContainer->getAudioResource(0) != nullptr)
    {
        return audioResourceContainer->getAudioResource(0)->getAudioTransportSource()->isPlaying();
    }
    
    return false;
    
}
