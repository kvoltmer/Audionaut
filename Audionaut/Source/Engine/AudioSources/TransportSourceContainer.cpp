//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "TransportSourceContainer.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Playback/Playback.h"

namespace audium {

std::shared_ptr<AudiumTransportSource> TransportSourceContainer::createAndAddTransportSource(AudioResource& audioResource,
                                                                                             std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource)
{
    auto transportSource = std::make_shared<AudiumTransportSource>(audioResource, std::move(audioFormatReaderSource));
    audioTransportSources.push_back(transportSource);
    return transportSource;
}

bool TransportSourceContainer::removeTransportSource(std::shared_ptr<AudiumTransportSource> audioTransportSource)
{
    if (std::erase(audioTransportSources, audioTransportSource) == 0)
        return false;

    playback->stopVoice(audioTransportSource, false);
    return true;
}

std::vector<std::shared_ptr<AudiumTransportSource>> TransportSourceContainer::getTransportSourcesForResource(const AudioResource &resource) const
{
    std::vector<std::shared_ptr<AudiumTransportSource>> result;

    for (const auto& transportSource : audioTransportSources)
    {
        if (&transportSource->getAudioResource() == &resource)
            result.push_back(transportSource);
    }
    return result;
}

void TransportSourceContainer::cleanup()
{
    playback->stopAllVoices();
    audioTransportSources.clear();
}

void TransportSourceContainer::prepareToPlay (int samplesPerBlockExpected,
                                              double sampleRate)
{
    for (const auto& transportSource : audioTransportSources)
        transportSource->prepareToPlay(samplesPerBlockExpected, sampleRate);

    applyChannelMapping();
}

std::shared_ptr<AudiumTransportSource> TransportSourceContainer::getTransportSourceAtIndex(int index) const
{
    if (index >= 0 && index < static_cast<int>(audioTransportSources.size()))
        return audioTransportSources[static_cast<size_t>(index)];

    return nullptr;
}

int TransportSourceContainer::getTransportSourceIndex(std::shared_ptr<AudiumTransportSource> searchTransportSource) const
{
    const auto it = std::ranges::find(audioTransportSources, searchTransportSource);
    if (it == audioTransportSources.end())
        return -1;

    return static_cast<int>(std::distance(audioTransportSources.begin(), it));
}

void TransportSourceContainer::applyChannelMapping()
{
    for (const auto& transportSource : audioTransportSources)
        transportSource->applyChannelMapping();
}

} // namespace audium
