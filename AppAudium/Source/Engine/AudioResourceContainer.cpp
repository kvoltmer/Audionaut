/*
  ==============================================================================

 AudioResourceContainer.cpp
    Created: 29 Jan 2023 12:37:15pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioResourceContainer.h"
#include "AudioPlayer.h"
#include "TransportSourceContainer.h"

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

AudioResourceContainer::AudioResourceContainer(std::shared_ptr<TransportSourceContainer> transportSourceContainer) :
    transportSourceContainer(transportSourceContainer)
{
    formatManager.registerBasicFormats();
    thread.startThread();
}

AudioResourceContainer::~AudioResourceContainer()
{
    audioResources.clear();
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
        /// TODO: create factory
        auto transportSource = transportSourceContainer->createNewTransportSource();
        auto audioPlayer = std::shared_ptr<AudioPlayer>(new AudioPlayer(transportSource,
                                                                        audioDeviceManager,
                                                                        inputSource.get(),
                                                                        formatManager,
                                                                        &thread));
        auto audioResource = std::shared_ptr<AudioResource>(new AudioResource(*this, url, inputSource.get(), formatManager, audioPlayer, thumbnailCache));
        audioResources.push_back(audioResource);
        inputSource.release();
        return audioResource;
    }
    
    return nullptr;
}


bool AudioResourceContainer::removeAudioResource (int atIndex)
{
    if (atIndex < 0 || atIndex >= audioResources.size())
        return false;
    
    transportSourceContainer->removeTransportSource(audioResources[atIndex]->getAudioTransportSource());
    audioResources.erase(audioResources.begin() + atIndex);
    
    return true;
}

std::shared_ptr<AudioResource> AudioResourceContainer::getAudioResource(int index) const
{
    if (index < getAudioResourceSize())
    {
        return audioResources[(std::size_t)index];
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

bool AudioResourceContainer::writeToStream (juce::OutputStream& outputStream)
{
    outputStream.writeInt((int)audioResources.size());
    for (auto & resource : audioResources)
    {
        resource->writeToStream(outputStream);
    }
    return true;
}

bool AudioResourceContainer::readFromStream (juce::InputStream& inputStream)
{
    audioResources.clear();
    auto numResources = inputStream.readInt();
    for (auto i = 0; i < numResources; i++)
    {
        auto inString = inputStream.readString();
        addAudioResource(juce::URL(inString));
    }
    
    return true;
}
