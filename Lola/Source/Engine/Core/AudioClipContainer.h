/*
  ==============================================================================

    AudioClipContainer.h
    Created: 27 May 2024 11:54:29am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <array>
#include <concepts>

#include "Engine/Core/DspClipData.h"

#define AUDIO_CLIP_ARRAY_SIZE 4096

// std::array, has a static size set at compile time.
// It does not have internal pointers and can therefore be copied simply by using memcpy.
// It therefore is trivial to copy (std::is_trivially_copyable)
template <size_t N = AUDIO_CLIP_ARRAY_SIZE>
class DspClipArray : public std::array<DspClipData, N>
{
};

class AudioClipContainer {
    
public:
    
    std::atomic<DspClipArray<AUDIO_CLIP_ARRAY_SIZE>> atomicDspClipArray;
    
    const std::vector<DspClipData> getDspClipDataVector() const
    {
        std::vector<DspClipData> result;
        auto data = atomicDspClipArray.load();
        for (auto i = 0; i < data.size(); i++)
        {
            if (!data[i].active) break;
            
            result.push_back(data[i]);
        }
        return result;
    }
    
};
