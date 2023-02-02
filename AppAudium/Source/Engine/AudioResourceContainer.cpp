/*
  ==============================================================================

 AudioResourceContainer.cpp
    Created: 29 Jan 2023 12:37:15pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioResourceContainer.h"

std::unique_ptr<juce::InputSource> makeAudioInputSource (const juce::URL& url)
{
   #if JUCE_ANDROID
    if (auto doc = AndroidDocument::fromDocument (url))
        return std::make_unique<AndroidDocumentInputSource> (doc);
   #endif

   #if ! JUCE_IOS
    if (url.isLocalFile())
        return std::make_unique<juce::FileInputSource> (url.getLocalFile());
   #endif

    return std::make_unique<juce::URLInputSource> (url);
}

AudioResourceContainer::AudioResourceContainer()
{
    formatManager.registerBasicFormats();
}


std::shared_ptr<AudioResource> AudioResourceContainer::addAudioResource (juce::URL url)
{
    if (auto inputSource = makeAudioInputSource (url))
    {
        auto audioResource = std::shared_ptr<AudioResource>(new AudioResource(url, formatManager));
        audioResources.push_back(audioResource);
        audioResource->getThumbnail().setSource (inputSource.release());
        return audioResource;
    }
    
    return nullptr;
}
