//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "VoiceSourceContainer.h"
#include "Engine/AudioSources/VoiceSource.h"
#include "Engine/Playback/Playback.h"

namespace audium {

std::shared_ptr<VoiceSource> VoiceSourceContainer::createAndAddVoiceSource(AudioResource& audioResource,
                                                                                             std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource)
{
    auto voiceSource = std::make_shared<VoiceSource>(audioResource, std::move(audioFormatReaderSource));
    voiceSources.push_back(voiceSource);
    return voiceSource;
}

bool VoiceSourceContainer::removeVoiceSource(std::shared_ptr<VoiceSource> voiceSource)
{
    if (std::erase(voiceSources, voiceSource) == 0)
        return false;

    playback->stopVoice(voiceSource, false);
    return true;
}

std::vector<std::shared_ptr<VoiceSource>> VoiceSourceContainer::getVoiceSourcesForResource(const AudioResource &resource) const
{
    std::vector<std::shared_ptr<VoiceSource>> result;

    for (const auto& voiceSource : voiceSources)
    {
        if (&voiceSource->getAudioResource() == &resource)
            result.push_back(voiceSource);
    }
    return result;
}

void VoiceSourceContainer::cleanup()
{
    playback->stopAllVoices();
    voiceSources.clear();
}

void VoiceSourceContainer::prepareToPlay (int samplesPerBlockExpected,
                                              double sampleRate)
{
    for (const auto& voiceSource : voiceSources)
        voiceSource->prepareToPlay(samplesPerBlockExpected, sampleRate);

    applyChannelMapping();
}

std::shared_ptr<VoiceSource> VoiceSourceContainer::getVoiceSourceAtIndex(int index) const
{
    if (index >= 0 && index < static_cast<int>(voiceSources.size()))
        return voiceSources[static_cast<size_t>(index)];

    return nullptr;
}

int VoiceSourceContainer::getVoiceSourceIndex(std::shared_ptr<VoiceSource> searchVoiceSource) const
{
    const auto it = std::ranges::find(voiceSources, searchVoiceSource);
    if (it == voiceSources.end())
        return -1;

    return static_cast<int>(std::distance(voiceSources.begin(), it));
}

void VoiceSourceContainer::applyChannelMapping()
{
    for (const auto& voiceSource : voiceSources)
        voiceSource->applyChannelMapping();
}

} // namespace audium
