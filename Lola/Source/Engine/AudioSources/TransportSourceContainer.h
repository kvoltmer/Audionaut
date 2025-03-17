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

#pragma once
#include <JuceHeader.h>

class AudioResourceContainer;
class AudioTrack;
class AudiumTransportSource;
class AudioResource;

namespace audium {
    class Playback;
}

class TransportSourceContainer
{
public:
    TransportSourceContainer(std::shared_ptr<audium::Playback> playback_) :
        playback(playback_)
    {}
    ~TransportSourceContainer() = default;
    
    void prepareToPlay (int samplesPerBlockExpected,
                        double sampleRate);
    void cleanup();

    std::shared_ptr<AudiumTransportSource> createAndAddTransportSource(AudioResource& audioResource,
                                                                       std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource);
    bool removeTransportSource(std::shared_ptr<AudiumTransportSource> audioTransportSource);
    
    std::vector<std::shared_ptr<AudiumTransportSource>> getTransportSourcesForResource(const AudioResource &resource) const;
    
    std::shared_ptr<AudiumTransportSource> getTransportSourceAtIndex(int index) const;
    int getTransportSourceIndex(std::shared_ptr<AudiumTransportSource> searchTransportSource) const;
     
    void applyChannelMapping();
    
private:
    
    std::shared_ptr<audium::Playback> playback;
    
    std::vector<std::shared_ptr<AudiumTransportSource>> audioTransportSources;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportSourceContainer)
};
