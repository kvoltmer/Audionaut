/*
  ==============================================================================

    TransportSourceProvider.cpp
    Created: 14 Jun 2023 6:12:34pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "TransportSourceProvider.h"
#include "AudioResourceContainer.h"
#include "AudiumTransportSource.h"

std::shared_ptr<AudiumTransportSource> TransportSourceProvider::createNewTransportSource()
{
    auto transportSource = std::shared_ptr<AudiumTransportSource> (new AudiumTransportSource());
    audioTransportSources.push_back(transportSource);
    return transportSource;
}

bool TransportSourceProvider::removeTransportSource(std::shared_ptr<AudiumTransportSource> audioTransportSource)
{
    auto it = std::find(audioTransportSources.begin(), audioTransportSources.end(), audioTransportSource);
    if (it != audioTransportSources.end())
    {
        audioTransportSources.erase(it);
        return true;
    }
    return false;
}

void TransportSourceProvider::setLocalPosition (double newPosition)
{
    for (auto & transportSource : audioTransportSources)
    {
        transportSource->setPosition(newPosition);
    }
}

double TransportSourceProvider::getLocalPosition() const
{
    if (audioTransportSources.size() > 0)
    {
        return audioTransportSources.front()->getCurrentPosition();
    }

    return 0;
}

void TransportSourceProvider::start()
{
    for (auto & transportSource : audioTransportSources)
    {
        transportSource->start();
    }
}

void TransportSourceProvider::stop()
{
    for (auto & transportSource : audioTransportSources)
    {
        // stop must not be used in the audio thread
        //transportSource->stop();
        
        // workaround: set the position to the very end
        transportSource->setPosition(transportSource->getLengthInSeconds());
    }
}

bool TransportSourceProvider::isPlaying() const
{
    if (audioTransportSources.size() > 0)
    {
        return audioTransportSources.front()->isPlaying();
    }
    return false;
}
