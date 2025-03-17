//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
        
    std::shared_ptr<audium::SelectionManager> selectionManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Selectable)
};

} // namespace audium
