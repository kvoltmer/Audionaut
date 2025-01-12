
#include "AudioBusInterface.h"

namespace audium
{

void AudioBusInterface::setPan(const int channelNumber, const float newPan)
{
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr, channelNumber, newPan] {
        ptr->setPan(channelNumber, newPan);
    });
}

void AudioBusInterface::setGain(const int channelNumber, const float newGain)
{
    auto ptr = audioBusRenderer.get();
    lockFreeCommander->fifo.push([ptr, channelNumber, newGain] {
        ptr->setGain(channelNumber, newGain);
    });
}

const float AudioBusInterface::getOutputLevel(const int channelNumber) const
{
    return audioBusRenderer->getOutputLevel(channelNumber);
}

} // namespace audium
