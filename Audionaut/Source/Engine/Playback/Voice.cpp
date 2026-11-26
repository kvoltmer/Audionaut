//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <cmath>

#include "Voice.h"
#include "Engine/AudioSources/VoiceSource.h"


namespace audium
{

int Voice::processAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    auto* source = voiceSource.load();
    if (processing.load() && source != nullptr) {

        info.clearActiveBufferRegion();
        source->getNextAudioBlock(info);
        const auto channel = source->getDestinationChannel();

        if (source->isStopped()) {
            processing.store(false);
            voiceSource.store(nullptr);
        }
        return channel;
    }
    return -1;
}

void Voice::start(VoiceSource* voiceSource_)
{
    voiceSource.store(voiceSource_);
    processing.store(true);
}

void Voice::stop(bool fadeOutLastBlock)
{
    if (auto* source = voiceSource.load())
        if (source->isPlaying())
            source->stop(fadeOutLastBlock);
}

void Voice::stopNow() noexcept
{
    if (auto* source = voiceSource.load())
        source->stop(false);

    processing.store(false);
    voiceSource.store(nullptr);
}


} // namespace audium


