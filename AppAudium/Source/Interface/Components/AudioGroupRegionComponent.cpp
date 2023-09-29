/*
  ==============================================================================

    AudioGroupRegionComponent.cpp
    Created: 28 Sep 2023 12:07:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "AudioGroupRegionComponent.h"
#include "Engine/AudioResourceGroup.h"
#include "Engine/AudioResourceContainer.h"
#include "Interface/Views/AudioRegionView.h"

//==============================================================================
AudioGroupRegionComponent::AudioGroupRegionComponent(std::shared_ptr<AudioResourceGroup> audioResourceGroup,
                                                     std::shared_ptr<AudioRegion> audioRegion,
                                                     std::shared_ptr<ZoomHandler> zoomHandler) :
    audioResourceGroup(audioResourceGroup)
{
    // this component doesn't handle mouse events
    setInterceptsMouseClicks(false, false);

    // create views
    auto audioResources = audioResourceGroup->getAudioResources();
    for (auto audioResource : audioResources)
    {
        auto view = std::shared_ptr<AudioRegionView>(new AudioRegionView(audioResource, zoomHandler, audioRegion));
        addAndMakeVisible(view.get());
        children.push_back(view);
    }
}

AudioGroupRegionComponent::~AudioGroupRegionComponent()
{
}

void AudioGroupRegionComponent::paint (juce::Graphics&)
{
    // children paint on top
}

void AudioGroupRegionComponent::resized()
{
    int top = 0;
    int count = 0;
    auto audioResources = audioResourceGroup->getAudioResources();
    for (auto audioResource : audioResources)
    {
        auto height = audioResource->getHeight();
        if (count < children.size())
        {
            auto child = children[count];
            if (child != nullptr)
            {
                child->setBounds(0, top, getWidth(), audioResource->getHeight());
            }
            count++;
        }
        top += height;
    }
}
