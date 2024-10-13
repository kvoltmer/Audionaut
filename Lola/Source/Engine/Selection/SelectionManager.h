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
    
    void selectItem(std::shared_ptr<Selectable> object, bool bSelected)
    {
        auto it = std::find(selectedObjects.begin(), selectedObjects.end(), object);
        
        if (it != selectedObjects.end())
            selectedObjects.erase(it);
        
        if (bSelected)
            selectedObjects.push_back(object);
    }
    
    bool isSomethingSelected() const { return selectedObjects.size() > 0; }
    
    std::vector<std::shared_ptr<Selectable>> getSelectedObjects() const { return selectedObjects; }
    
    void clear() { selectedObjects.clear(); }
    
    void deselectAll();
    
    void copySelectedToClipboard();
    
private:
        
    std::vector<std::shared_ptr<Selectable>> selectedObjects;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectionManager)
};

} // namespace audium
