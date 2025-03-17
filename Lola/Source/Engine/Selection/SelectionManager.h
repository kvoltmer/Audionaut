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
#include <nlohmann/json.hpp>

#include "SelectionContext.h"

using json = nlohmann::json;

class AudiumEngine;

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
    
    const std::vector<std::shared_ptr<Selectable>> &getSelectedObjects() const { return selectedObjects; }
    
    void clear() { selectedObjects.clear(); }
    
    void deselectAll();
    
    const SelectionContextType getSelectionContext() const;
    
    void copySelectedToClipboard();
    bool canParseFromClipboard();
    void pasteFromClipboard(std::shared_ptr<AudiumEngine> audiumEngine, bool duplicateAction);
    
private:
        
    void pastePlayListItems(const json &input,
                            std::shared_ptr<AudiumEngine> audiumEngine,
                            bool duplicateAction);
    
    std::vector<std::shared_ptr<Selectable>> selectedObjects;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectionManager)
};

} // namespace audium
