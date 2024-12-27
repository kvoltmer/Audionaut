

#include <cmath>

#include "Voice.h"
#include "Engine/AudioSources/AudiumTransportSource.h"


namespace audium
{

void Voice::processAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    if (processing.load() && transportSource != nullptr)
    {
        transportSource->getNextAudioBlock(info);
        
        if (transportSource == nullptr ||
            transportSource->isStopped())
            stop();
    }
}

void Voice::start(std::shared_ptr<AudiumTransportSource> transportSource_)
{
    transportSource = transportSource_;
    processing.store(true);
}

void Voice::stop()
{
    processing.store(false);
    transportSource = nullptr;
}


} // namespace audium


