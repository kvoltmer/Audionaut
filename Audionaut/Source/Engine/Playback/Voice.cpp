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
    if (processing.load() && voiceSource != nullptr) {
        
        info.clearActiveBufferRegion();
        voiceSource->getNextAudioBlock(info);
        
        if (voiceSource == nullptr ||
            voiceSource->isStopped()) {
            
            processing.store(false);
            voiceSource = nullptr;
        }
    }
}

void Voice::start(std::shared_ptr<VoiceSource> voiceSource_)
{
    voiceSource = voiceSource_;
    processing.store(true);
}

void Voice::stop(bool fadeOutLastBlock)
{
    if (voiceSource != nullptr &&
        voiceSource->isPlaying())
        
        voiceSource->stop(fadeOutLastBlock);
}


} // namespace audium


