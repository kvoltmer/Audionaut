/*
  ==============================================================================

    EngineAccess.cpp
    Created: 1 Feb 2023 4:46:46pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Util/EngineAccess.h"

std::shared_ptr<AudiumEngine> getAudiumEngine(juce::Component* component)
{
    auto topLevelComponent = component->getTopLevelComponent();
    jassert(topLevelComponent);
    auto mainWindow = dynamic_cast<AudiumMainWindow*>(topLevelComponent);
    jassert(mainWindow);
    return mainWindow->getEngine();
}
