//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

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
