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

#include <array>
#include <concepts>

#include "Engine/Core/DspClipData.h"
#include "Engine/Core/LockFreeContainer.h"

namespace audium {

class AudioClipContainer {
    
public:
    
    AudioClipContainer(int capacity) :
        dspClips(capacity)
    {
    }
    
    ~AudioClipContainer() = default;
    
    void clear()
    {
        dspClips.getProducerObjects().clear();
    }
    
    void push_back(DspClipData clip)
    {
        dspClips.getProducerObjects().push_back(clip);
    }
    
    void commit()
    {
        dspClips.commit();
    }
    
    bool pull()
    {
        return dspClips.pull();
    }
    
    const std::vector<DspClipData> &getConsumerObjects()
    {
        return dspClips.getConsumerObjects();
    }
    
private:
    
    audium::LockFreeContainer<DspClipData> dspClips;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioClipContainer)
    
};

} // namespace audium
