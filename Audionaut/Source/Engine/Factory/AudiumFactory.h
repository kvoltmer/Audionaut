//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"

namespace audium {

/**
 * @class AudiumFactory
 * @brief Factory class for creating instances of the `AudiumEngine`.
 *
 * This class provides a centralized way to create and initialize the `AudiumEngine`,
 * ensuring consistency and simplifying the management of engine instances.
 */
class AudiumFactory {
    
public:
    /**
     * @brief Default constructor for `AudiumFactory`.
     */
    AudiumFactory() = default;
    
    /**
     * @brief Creates a new instance of the `AudiumEngine`.
     * @return A shared pointer to the created `AudiumEngine` instance.
     */
    static std::shared_ptr<AudiumEngine> createAudiumEngine();
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumFactory)
};

} // namespace audium
