/*
  ==============================================================================

    AtomicAdapter.h
    Created: 27 May 2024 11:54:29am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "Engine/Core/DspClipData.h"

#define AUDIO_CLIP_ARRAY_SIZE 64

class AudioClipContainer {
    
public:
    
    // std::array, has a static size set at compile time. It does not have internal pointers and can therefore be copied simply by using memcpy. It therefore is trivial to copy (std::is_trivially_copyable)
    typedef class std::array<DspClipData, AUDIO_CLIP_ARRAY_SIZE> tDspClipArray;
    
    std::atomic<tDspClipArray> atomicDspClipArray;
    
};
