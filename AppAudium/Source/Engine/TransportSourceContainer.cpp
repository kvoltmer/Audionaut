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

void TransportSourceContainer::setLocalPosition (std::shared_ptr<AudioGroup> group, double newPosition)
{
    for (auto & transportSource : audioTransportSources)
    {
        if (group == transportSource->getAudioGroup())
            transportSource->setPosition(newPosition);
    }
}

double TransportSourceContainer::getLocalPosition(std::shared_ptr<AudioGroup> group) const
{
    for (auto & transportSource : audioTransportSources)
    {
        if (group == transportSource->getAudioGroup())
            return transportSource->getCurrentPosition();
    }
    return 0;
}

void TransportSourceContainer::start(std::shared_ptr<AudioGroup> group)
{
    for (auto & transportSource : audioTransportSources)
    {
        if (group == transportSource->getAudioGroup())
            transportSource->start();
    }
}

void TransportSourceContainer::stop(std::shared_ptr<AudioGroup> group)
{
    for (auto & transportSource : audioTransportSources)
    {
        if (group == transportSource->getAudioGroup())
        {
            // stop must not be used in the audio thread
            //transportSource->stop();
            
            // workaround: set the position to the very end
            transportSource->setPosition(transportSource->getLengthInSeconds());
        }
    }
}

bool TransportSourceContainer::isPlaying(std::shared_ptr<AudioGroup> group) const
{
    for (auto & transportSource : audioTransportSources)
    {
        if (group == transportSource->getAudioGroup())
            return transportSource->isPlaying();
    }
    return false;
}
