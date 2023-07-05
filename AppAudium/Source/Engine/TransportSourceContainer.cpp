/*
  ==============================================================================

    TransportSourceContainer.cpp
    Created: 14 Jun 2023 6:12:34pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "TransportSourceContainer.h"
#include "AudioResourceContainer.h"

std::shared_ptr<juce::AudioTransportSource> TransportSourceContainer::createNewTransportSource()
{
    auto transportSource = std::shared_ptr<juce::AudioTransportSource> (new juce::AudioTransportSource());
    audioTransportSources.push_back(transportSource);
    return transportSource;
}

void TransportSourceContainer::setPosition (double newPosition)
{
    for (auto & transportSource : audioTransportSources)
    {
        transportSource->setPosition(newPosition);
    }

    std::cout << "setPosition " << newPosition << std::endl;
}

double TransportSourceContainer::getCurrentPosition() const
{
    if (audioTransportSources.size() > 0)
    {
        return audioTransportSources.front()->getCurrentPosition();
    }

    return 0;
}

void TransportSourceContainer::start()
{
    for (auto & transportSource : audioTransportSources)
    {
        transportSource->start();
    }
}


void TransportSourceContainer::stop()
{
    for (auto & transportSource : audioTransportSources)
    {
        transportSource->stop();
    }
}

bool TransportSourceContainer::isPlaying() const
{
    if (audioTransportSources.size() > 0)
    {
        return audioTransportSources.front()->isPlaying();
    }
    
    return false;
}
