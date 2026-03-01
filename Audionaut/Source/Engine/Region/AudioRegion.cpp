//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudioRegion.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioResourceFactory.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/Region/AudioRegionContainer.h"

namespace audium {

void AudioRegion::sendActionMessage (const juce::String& message) const
{
    audioTrack->getAudioTrackContainer().sendActionMessage(message);
}

bool AudioRegion::writeToStream (juce::OutputStream& outputStream)
{
    return audium::Streamable::writeToStream(outputStream);
}

bool AudioRegion::readFromStream (juce::InputStream& inputStream, bool rebuild)
{
    if (audium::Streamable::readFromStream(inputStream)) {
        sendActionMessage(updateAll);
        return true;
    }
    return false;
}

bool AudioRegion::writeToJson (json& output)
{
    // make sure all id's are up to date
    auto shared_ptr     = std::dynamic_pointer_cast<AudioRegion> (getSharedPtr());
    data.region_id      = resourceGroup->getAudioRegionContainer()->getRegionId(shared_ptr);
    data.track_id       = audioTrack->getId();
    data.resource_group_id   = audioTrack->resourceGroupContainer->getIndex(resourceGroup);
    data.recording      = isRecording();
    
    output = data;
    return true;
}

int AudioRegion::getSizeInUnits()
{
    return 1;
}

const AudioRegionData::tRange AudioRegion::getRegionData(audium::TimeContextType context) const
{
    if (context == audium::seconds) {
        return data.range;
    }
    else if (context == audium::clocks) {
        return tempoProvider->secondsToClocks(data.range);
    }
    
    jassertfalse;
    return AudioRegionData::tRange();
}

void AudioRegion::setRegionData(const AudioRegionData::tRange newRegionData, audium::TimeContextType context)
{
    if (!newRegionData.isEmpty() &&
        newRegionData.getStart() <= newRegionData.getEnd()) {
        if (context == audium::seconds) {
            data.range = newRegionData;
        }
        else if (context == audium::clocks) {
            data.range = tempoProvider->clocksToSeconds(newRegionData);
        }
    }
    else {
        std::cout << "AudioRegion::setRegionData invalid range\n" << std::endl;
    }
}

bool AudioRegion::validateData(AudioRegionData::tRange& newData, audium::TimeContextType context)
{
    bool result = false;
    
    if (newData.getStart() < 0.0) {
        newData = newData.movedToStartAt(0.0);
        result |= true;
    }
    
    if (newData.getEnd() > resourceGroup->getMaxLength(context)) {
        newData = newData.movedToEndAt(resourceGroup->getMaxLength(context));
        result |= true;
    }
    return result;
}

void AudioRegion::setRegionStart(double newStart, audium::TimeContextType context)
{
    if (newStart <= getRegionData(context).getEnd()) {
        setRegionData(AudioRegionData::tRange(newStart, getRegionData(context).getEnd()), context);
    }
}

void AudioRegion::setRegionEnd(double newEnd, audium::TimeContextType context)
{
    if (newEnd >= getRegionData(context).getStart()) {
        setRegionData(AudioRegionData::tRange(getRegionData(context).getStart(), newEnd), context);
    }
}

void AudioRegion::setRegionLength(double newLength, audium::TimeContextType context)
{
    setRegionData(getRegionData(context).withLength(newLength), context);
}

std::vector<std::shared_ptr<AudioResource>> AudioRegion::getAudioResources() const
{
    return audioTrack->getAudioResourceContainer().getAudioResourcesForResourceGroup(resourceGroup.get());
}

bool AudioRegion::deleteAssociatedItems()
{
    return getAudioTrack()->getPlayListContainer()->deleteAssociatedItems(this);
}

void AudioRegion::setGain(int channel, double newGain, bool continous)
{
    if (channel >= 0) {
        while (channel >= data.gain_vector.size()) {
            data.gain_vector.push_back(1.0);
        }
        data.gain_vector[channel] = newGain;
        
        if (continous) {
            
            auto resource = getResourceGroup()->getAudioResourceAtChannel(channel);
            auto sources = audioTrack->getTransportSourceContainer()->getTransportSourcesForResource(*resource.get());
            
            // TODO: this is not thread save
            for (auto source : sources)
                if (source->isPlaying())
                    source->getAudioTransportSource()->setGain(newGain);
        }
    }
}

double AudioRegion::getGain(int channel) const
{
    if (channel >= 0 && channel < data.gain_vector.size()) {
        return data.gain_vector[channel];
    }
    return 1.0;
}

void AudioRegion::onDeleteChannel(int channel)
{
    if (channel < 0 || channel >= data.gain_vector.size())
        return;
    // remove channel from gain vector
    data.gain_vector.erase(data.gain_vector.begin() + static_cast<size_t>(channel));
}

double AudioRegion::getResourcesMaxSampleRate() const
{
    auto sr = 0.0;
    for (auto res : getAudioResources()) {
        sr = std::max(sr, res->getSampleRate());
    }
    jassert(sr > 0.0);
    return sr;
}

int AudioRegion::getResourcesMaxBitDepth() const
{
    unsigned int bitDepth = 0;
    for (auto res : getAudioResources()) {
        bitDepth = std::max(bitDepth, res->getBitDepth());
    }
    jassert(bitDepth > 0.0);
    return bitDepth;
}

bool AudioRegion::isRecording() const
{
    for (auto resource : getAudioResources())
        if (resource->isRecording())
            return true;
    
    return false;
}

} // namespace audium

