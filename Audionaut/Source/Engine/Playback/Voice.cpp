//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <cmath>

#include "Voice.h"
#include "Engine/AudioSources/AudiumTransportSource.h"


namespace audium
{

void Voice::processAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    if (processing.load() && transportSource != nullptr) {
        
        info.clearActiveBufferRegion();
        transportSource->getNextAudioBlock(info);
        
        if (transportSource == nullptr ||
            transportSource->isStopped()) {
            
            processing.store(false);
            transportSource = nullptr;
        }
    }
}

void Voice::start(std::shared_ptr<AudiumTransportSource> transportSource_)
{
    transportSource = transportSource_;
    processing.store(true);
}

void Voice::stop(bool fadeOutLastBlock)
{
    if (transportSource != nullptr &&
        transportSource->isPlaying())
        
        transportSource->stop(fadeOutLastBlock);
}


} // namespace audium


