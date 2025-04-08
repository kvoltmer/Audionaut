//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

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
    
    std::shared_ptr<Selectable> getSharedPtr() { return shared_from_this(); }

    std::shared_ptr<const Selectable> getSharedPtr() const { return shared_from_this(); }
    
    virtual void cleanup() {}
    
private:
    bool selected = false;
        
    std::shared_ptr<SelectionManager> selectionManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Selectable)
};

} // namespace audium
