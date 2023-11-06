/*
  ==============================================================================

    TransportSourceContainer.cpp
    Created: 14 Jun 2023 6:12:34pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "TransportSourceContainer.h"
#include "AudioResourceContainer.h"
#include "AudiumTransportSource.h"

std::shared_ptr<AudiumTransportSource> TransportSourceContainer::createNewTransportSource()
{
    auto transportSource = std::shared_ptr<AudiumTransportSource> (new AudiumTransportSource());
    audioTransportSources.push_back(transportSource);
    return transportSource;
}

bool TransportSourceContainer::removeTransportSource(std::shared_ptr<AudiumTransportSource> audioTransportSource)
{
    auto it = std::find(audioTransportSources.begin(), audioTransportSources.end(), audioTransportSource);
    if (it != audioTransportSources.end())
    {
        audioTransportSources.erase(it);
        return true;
    }
    return false;
}

void TransportSourceContainer::setLocalPosition (double seconds, int startSample)
{
    for (auto & transportSource : audioTransportSources)
    {
        transportSource->schedulePosition(seconds, startSample);
    }
}

double TransportSourceContainer::getLocalPosition() const
{
    for (auto & transportSource : audioTransportSources)
    {
        return transportSource->getCurrentPosition();
    }
    return 0;
}

void TransportSourceContainer::startPlaying()
{
    for (auto & transportSource : audioTransportSources)
    {
        transportSource->start();
    }
    playing = true;
}

void TransportSourceContainer::stopPlaying()
{
    for (auto & transportSource : audioTransportSources)
    {
        // stop must not be used in the audio thread
        //transportSource->stop();
        
        // workaround: set the position to the very end
        if (transportSource->isPlaying())
        {
            transportSource->setPosition(transportSource->getLengthInSeconds());
        }
    }
    playing = false;
}

bool TransportSourceContainer::isPlaying() const
{
    return playing;
}
