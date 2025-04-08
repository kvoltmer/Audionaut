//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

#include "SelectionContext.h"

using json = nlohmann::json;

namespace audium {

class Selectable;
class AudiumEngine;

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
