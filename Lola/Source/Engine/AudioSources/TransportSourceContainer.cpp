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

#include "TransportSourceContainer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Playback/Playback.h"

namespace audium {

std::shared_ptr<AudiumTransportSource> TransportSourceContainer::createAndAddTransportSource(AudioResource& audioResource,
                                                                                             std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource)
{
    auto transportSource = std::shared_ptr<AudiumTransportSource> (new AudiumTransportSource(audioResource, audioFormatReaderSource));
    audioTransportSources.push_back(transportSource);
    return transportSource;
}

bool TransportSourceContainer::removeTransportSource(std::shared_ptr<AudiumTransportSource> audioTransportSource)
{
    playback->stopVoice(audioTransportSource);
    
    return std::erase_if(audioTransportSources, [audioTransportSource](const auto& item) {
        return item == audioTransportSource;
    }) > 0;
}

std::vector<std::shared_ptr<AudiumTransportSource>> TransportSourceContainer::getTransportSourcesForResource(const AudioResource &resource) const
{
    std::vector<std::shared_ptr<AudiumTransportSource>> result;
    
    for (auto transportSource : audioTransportSources) {
        
        if (&transportSource->getAudioResource() == &resource)
            result.push_back(transportSource);
    }
    return result;
}

void TransportSourceContainer::cleanup()
{
    playback->stopAllVoices();
    audioTransportSources.clear();
}

void TransportSourceContainer::prepareToPlay (int samplesPerBlockExpected,
                                              double sampleRate)
{
    for (auto transportSource : audioTransportSources)
    {
        transportSource->prepareToPlay(samplesPerBlockExpected, sampleRate);
    }
    
    applyChannelMapping();
}

std::shared_ptr<AudiumTransportSource> TransportSourceContainer::getTransportSourceAtIndex(int index) const
{
    if (index >= 0 &&
        index < (int)audioTransportSources.size())
    {
        return audioTransportSources[index];
    }
    return nullptr;
}

int TransportSourceContainer::getTransportSourceIndex(std::shared_ptr<AudiumTransportSource> searchObject) const
{
    auto it = std::find(audioTransportSources.begin(), audioTransportSources.end(), searchObject);
    if (it != audioTransportSources.end())
        return static_cast<int>(std::distance(audioTransportSources.begin(), it));
    
    return -1;
}

void TransportSourceContainer::applyChannelMapping()
{
    for (auto transportSource : audioTransportSources)
    {
        transportSource->applyChannelMapping();
    }
}

} // namespace audium
