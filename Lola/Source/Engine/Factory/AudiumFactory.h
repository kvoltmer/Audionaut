//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"

namespace audium {

/// The Audium factory
class AudiumFactory {
    
public:
    AudiumFactory() = default;
    
    static std::shared_ptr<AudiumEngine> createAudiumEngine();
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumFactory)
};

} // namespace audium
