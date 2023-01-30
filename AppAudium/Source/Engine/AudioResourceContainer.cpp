/*
  ==============================================================================

 AudioResourceContainer.cpp
    Created: 29 Jan 2023 12:37:15pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioResourceContainer.h"


 void AudioResourceContainer::addAudioResource (juce::URL resource)
{
     
     audioResources.push_back(std::unique_ptr<AudioResource>(new AudioResource(resource)));
     
//    if (loadURLIntoTransport (resource))
//        currentAudioFile = std::move (resource);
//
//    //zoomSlider.setValue (0, dontSendNotification);
//    waveFormComponent->setURL (currentAudioFile);
}
