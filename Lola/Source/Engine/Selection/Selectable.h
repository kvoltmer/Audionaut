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

class Selectable
{

public:
    Selectable(std::shared_ptr<SelectionManager> selectionManager) :
        selectionManager(selectionManager)
    {
    }
    
    virtual ~Selectable() = default;
    
    virtual void setSelected(bool bSelected, bool selectChildren)
    {
        selected = bSelected;
        selectionManager->selectItem(this, bSelected);
    }
    
    virtual bool isSelected() const { return selected; }
    
    
private:
    bool selected = false;
        
    std::shared_ptr<audium::SelectionManager> selectionManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Selectable)
};

} // namespace audium
