/*
  ==============================================================================

 AudioResourceContainer.cpp
    Created: 29 Jan 2023 12:37:15pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioResourceContainer.h"
#include "AudioPlayer.h"

static std::unique_ptr<juce::InputSource> makeAudioInputSource (const juce::URL& url)
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
    thread.startThread();

}

void AudioResourceContainer::initializeAudioDevice()
{
    /** Resets everything to a default device setup, clearing any stored settings. */
    auto result = audioDeviceManager.initialiseWithDefaultDevices (0, 2);
    std::cout << result.toStdString() << std::endl;
}

std::shared_ptr<AudioResource> AudioResourceContainer::addAudioResource (juce::URL url)
{
    if (auto inputSource = makeAudioInputSource (url))
    {
        auto audioPlayer = std::shared_ptr<AudioPlayer>(new AudioPlayer(audioDeviceManager, inputSource.get(), formatManager, &thread));
        auto audioResource = std::shared_ptr<AudioResource>(new AudioResource(*this, url, inputSource.get(), formatManager, audioPlayer, thumbnailCache));
        audioResources.push_back(audioResource);
        inputSource.release();
        return audioResource;
    }
    
    return nullptr;
}

double AudioResourceContainer::getTotalLengthMax() const
{
    double length = 0;// 420;
    
    for (auto & element : audioResources)
    {
        length = std::max(length, element->getThumbnail().getTotalLength());
    }
    return length;
}

void AudioResourceContainer::start()
{
    for (auto & element : audioResources)
    {
        element->start();
    }
}

void AudioResourceContainer::stop()
{
    for (auto & element : audioResources)
    {
        element->stop();
    }
}

void AudioResourceContainer::playStop()
{
    isPlaying ? stop() : start();
    isPlaying = !isPlaying;
}

