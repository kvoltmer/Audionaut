/*
  ==============================================================================

    AudiumFactory.h
    Created: 27 Jun 2023 10:41:00am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <memory>
#include <JuceHeader.h>
#include "AudiumEngine.h"
#include "AudioResourceContainer.h"
#include "AudioRegionContainer.h"



/// The Audium engine
class AudiumFactory {
    
public:
    AudiumFactory() = default;
    
    static std::shared_ptr<AudiumEngine> createAudiumEngine();
    
private:
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumFactory)
};
