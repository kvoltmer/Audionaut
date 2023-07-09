/*
  ==============================================================================

    TransportSourceProvider.cpp
    Created: 14 Jun 2023 6:12:34pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "TransportSourceProvider.h"
#include "AudioResourceContainer.h"

std::shared_ptr<juce::AudioTransportSource> TransportSourceProvider::createNewTransportSource()
{
    auto transportSource = std::shared_ptr<juce::AudioTransportSource> (new juce::AudioTransportSource());
    audioTransportSources.push_back(transportSource);
    return transportSource;
}

void TransportSourceProvider::removeTransportSource(std::shared_ptr<juce::AudioTransportSource> audioTransportSource)
{
    auto it = std::find(audioTransportSources.begin(), audioTransportSources.end(), audioTransportSource);
    if (it != audioTransportSources.end())
    {
        audioTransportSources.erase(it);
    }
}

void TransportSourceProvider::setPosition (double newPosition)
{
    for (auto & transportSource : audioTransportSources)
    {
        transportSource->setPosition(newPosition);
    }

    std::cout << "setPosition " << newPosition << std::endl;
}

double TransportSourceProvider::getCurrentPosition() const
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
        transportSource->stop();
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

bool TransportSourceProvider::playStop()
{
    if (isPlaying())
    {
        stop();
        return false;
    }
    else
    {
        start();
        return true;
    }
}
