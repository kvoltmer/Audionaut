//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "VoiceSourceContainer.h"
#include "VoiceSource.h"
#include "Engine/Playback/Playback.h"

namespace audium {

std::shared_ptr<VoiceSource> VoiceSourceContainer::createAndAddVoiceSource(AudioResource& audioResource,
                                                                           std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource)
{
    auto voiceSource = std::make_shared<VoiceSource>(audioResource, std::move(audioFormatReaderSource));
    // append only: a freed slot is never reused, so an index means the same
    // voice in every snapshot generation that contains it
    voiceSources.push_back(voiceSource);
    return voiceSource;
}

bool VoiceSourceContainer::removeVoiceSource(std::shared_ptr<VoiceSource> voiceSource)
{
    if (voiceSource == nullptr)
        return false;

    const auto it = std::ranges::find(voiceSources, voiceSource);
    if (it == voiceSources.end())
        return false;

    playback->stopVoice(voiceSource.get(), false);

    // keep the object alive until the audio thread has let go (see commit)
    retiredPending.push_back(std::move(*it));
    *it = nullptr;
    return true;
}

std::vector<std::shared_ptr<VoiceSource>> VoiceSourceContainer::getVoiceSourcesForResource(const AudioResource &resource) const
{
    std::vector<std::shared_ptr<VoiceSource>> result;

    for (const auto& voiceSource : voiceSources)
    {
        if (voiceSource != nullptr && &voiceSource->getAudioResource() == &resource)
            result.push_back(voiceSource);
    }
    return result;
}

void VoiceSourceContainer::cleanup()
{
    playback->stopAllVoices();

    for (auto& voiceSource : voiceSources)
        if (voiceSource != nullptr)
            retiredPending.push_back(std::move(voiceSource));

    voiceSources.clear(); // indices restart: the next commit publishes an empty set
    commit();
}

void VoiceSourceContainer::prepareToPlay (int samplesPerBlockExpected,
                                          double sampleRate)
{
    for (const auto& voiceSource : voiceSources)
        if (voiceSource != nullptr)
            voiceSource->prepareToPlay(samplesPerBlockExpected, sampleRate);

    applyChannelMapping();
}

int VoiceSourceContainer::getVoiceSourceIndex(std::shared_ptr<VoiceSource> searchVoiceSource) const
{
    if (searchVoiceSource == nullptr)
        return -1;

    const auto it = std::ranges::find(voiceSources, searchVoiceSource);
    if (it == voiceSources.end())
        return -1;

    return static_cast<int>(std::distance(voiceSources.begin(), it));
}

void VoiceSourceContainer::applyChannelMapping()
{
    for (const auto& voiceSource : voiceSources)
        if (voiceSource != nullptr)
            voiceSource->applyChannelMapping();
}

void VoiceSourceContainer::commit()
{
    // 1. sources excluded from the previous commit may go once the audio
    //    thread has pulled it and no voice still renders them
    releaseRetired();

    // 2. what was removed since then is excluded from the snapshot below
    retiredCommitted.insert(retiredCommitted.end(),
                            std::make_move_iterator(retiredPending.begin()),
                            std::make_move_iterator(retiredPending.end()));
    retiredPending.clear();

    // 3. publish
    auto& producer = snapshot.getProducerObjects();
    producer.clear();
    for (const auto& voiceSource : voiceSources)
        producer.push_back(voiceSource.get());
    snapshot.commit();
}

void VoiceSourceContainer::releaseRetired()
{
    if (retiredCommitted.empty() || ! snapshot.isPulled())
        return;

    std::erase_if(retiredCommitted, [this] (const std::shared_ptr<VoiceSource>& voiceSource) {
        // a Voice may still be draining the stop we requested on removal
        return ! playback->isVoiceSourceActive(voiceSource.get());
    });
}

bool VoiceSourceContainer::pull()
{
    return snapshot.pull();
}

VoiceSource* VoiceSourceContainer::getOwnedVoiceSourceAtIndex(int index) const noexcept
{
    if (index >= 0 && index < static_cast<int>(voiceSources.size()))
        return voiceSources[static_cast<size_t>(index)].get();

    return nullptr;
}

VoiceSource* VoiceSourceContainer::getVoiceSourceAtIndex(int index) const noexcept
{
    const auto& objects = snapshot.getConsumerObjects();
    if (index >= 0 && index < static_cast<int>(objects.size()))
        return objects[static_cast<size_t>(index)];

    return nullptr;
}

} // namespace audium
