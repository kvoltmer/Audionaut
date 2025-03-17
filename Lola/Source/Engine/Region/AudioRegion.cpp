//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "AudioRegion.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioResourceFactory.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/Region/AudioRegionContainer.h"

AudioRegion::~AudioRegion()
{
}

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
    if (audium::Streamable::readFromStream(inputStream))
    {
        sendActionMessage(updateAll);
        return true;
    }
    return false;
}

bool AudioRegion::writeToJson (json& output)
{
    // make sure all id's are up to date
    auto shared_ptr = std::dynamic_pointer_cast<AudioRegion> (getSharedPtr());
    data.region_id      = audioSubGroup->getAudioRegionContainer()->getRegionId(shared_ptr);
    data.track_id       = audioTrack->getId();
    data.sub_group_id   = audioTrack->audioSubGroupContainer->getIndex(audioSubGroup);
     
    
    output = data;
    return true;
}

bool AudioRegion::readFromJson (json& input, bool rebuild)
{
    data = input;
    return true;
}


int AudioRegion::getSizeInUnits()
{
    return 1;
}

const AudioRegionData::tRange AudioRegion::getRegionData(audium::TimeContextType context) const
{
    if (context == audium::seconds)
    {
        return data.range;
    }
    else if (context == audium::clocks)
    {
        return tempoProvider->secondsToClocks(data.range);
    }
    
    jassertfalse;
    return AudioRegionData::tRange();
}

void AudioRegion::setRegionData(const AudioRegionData::tRange newRegionData, audium::TimeContextType context)
{
    jassert(!newRegionData.isEmpty());
    jassert(newRegionData.getStart() <= newRegionData.getEnd());

    if (context == audium::seconds)
    {
        data.range = newRegionData;
    }
    else if (context == audium::clocks)
    {
        data.range = tempoProvider->clocksToSeconds(newRegionData);
    }
}

bool AudioRegion::validateData(AudioRegionData::tRange& newData, audium::TimeContextType context)
{
    bool result = false;
    
    if (newData.getStart() < getAudioResourceStart(context))
    {
        newData = newData.movedToStartAt(getAudioResourceStart(context));
        result |= true;
    }
    
    if (newData.getEnd() > getAudioResourceEnd(context))
    {
        newData = newData.movedToEndAt(getAudioResourceEnd(context));
        result |= true;
    }
    return result;
}

double AudioRegion::getAudioResourceStart(audium::TimeContextType context) const
{
    return audioSubGroup->getRegionData(context).getStart();
}

double AudioRegion::getAudioResourceEnd(audium::TimeContextType context) const
{
    return audioSubGroup->getRegionData(context).getEnd();
}

void AudioRegion::setRegionStart(double newStart, audium::TimeContextType context)
{
    if (newStart <=  getRegionData(context).getEnd())
    {
        setRegionData(AudioRegionData::tRange(newStart, getRegionData(context).getEnd()), context);
    }
}

void AudioRegion::setRegionEnd(double newEnd, audium::TimeContextType context)
{
    if (newEnd >=  getRegionData(context).getStart())
    {
        setRegionData(AudioRegionData::tRange(getRegionData(context).getStart(), newEnd), context);
    }
}

void AudioRegion::setRegionLength(double newLength, audium::TimeContextType context)
{
    setRegionData(getRegionData(context).withLength(newLength), context);
}

std::vector<std::shared_ptr<AudioResource>> AudioRegion::getAudioResources() const
{
    return audioTrack->getAudioResourceContainer().getAudioResourcesForSubGroup(audioSubGroup.get());
}

bool AudioRegion::deleteAssociatedItems()
{
    return getAudioTrack()->getPlayListContainer()->deleteAssociatedItems(this);
}

void AudioRegion::setGain(int channel, double newGain, bool continous)
{
    if (channel >= 0) {
        if (channel >= data.gain_vector.size()) {
            data.gain_vector.resize(channel + 1, 1.0);
        }
        data.gain_vector[channel] = newGain;
        
        if (continous) {

            auto resource = getAudioSubGroup()->getAudioResourceAtChannel(channel);
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
    for (auto i = channel; i < data.gain_vector.size(); i++) {
        if (i + 1 < data.gain_vector.size())
            data.gain_vector[i] = data.gain_vector[i + 1];
    }
}
