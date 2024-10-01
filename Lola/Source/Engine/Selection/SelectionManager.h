/*
  ==============================================================================

    SelectionManager.h
    Created: 1 Oct 2024 2:06:18pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace audium
{

class Selectable;

class SelectionManager
{

public:
    SelectionManager() = default;
    ~SelectionManager() = default;
    
    void selectItem(Selectable* object, bool bSelected)
    {
        std::cout << "selectItem: " << object << " " << bSelected <<  std::endl;
    }
    
private:
        
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectionManager)
};

} // namespace audium
