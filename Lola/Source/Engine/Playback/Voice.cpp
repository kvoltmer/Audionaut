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

void Voice::stop()
{
    if (transportSource != nullptr)
        transportSource->getAudioTransportSource()->stop();
}


} // namespace audium


