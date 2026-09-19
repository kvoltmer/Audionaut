//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <cmath>

#include "Voice.h"
#include "Engine/AudioSources/VoiceSource.h"


namespace audium
{

void Voice::processAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    auto* source = voiceSource.load();
    if (processing.load() && source != nullptr) {
        
        info.clearActiveBufferRegion();
        source->getNextAudioBlock(info);
        
        if (source->isStopped()) {
            processing.store(false);
            voiceSource.store(nullptr);
        }
    }
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


} // namespace audium


