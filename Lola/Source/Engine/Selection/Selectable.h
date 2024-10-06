/*
  ==============================================================================

    Selectable.h
    Created: 1 Oct 2024 2:06:03pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "SelectionManager.h"

namespace audium
{

class SelectionManager;

class Selectable : public std::enable_shared_from_this<Selectable>
{

public:
    Selectable(std::shared_ptr<SelectionManager> selectionManager) :
        selectionManager(selectionManager)
    {
    }
    
    virtual ~Selectable() = default;
    
    virtual void setSelected(bool bSelected, bool selectChildren = false) {
        if (selected != bSelected) {
            selected = bSelected;
            jassert(selectionManager);
            selectionManager->selectItem(getSharedPtr(), bSelected);
        }
    }
    
    virtual bool isSelected() const { return selected; }
    
    std::shared_ptr<Selectable> getSharedPtr()
    {
        return shared_from_this();
    }
    
    virtual void cleanup() {}
    
private:
    bool selected = false;
        
    std::shared_ptr<audium::SelectionManager> selectionManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Selectable)
};

} // namespace audium
