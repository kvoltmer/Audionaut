/*
  ==============================================================================

    SelectionManager.cpp
    Created: 6 Oct 2024 11:59:59am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "SelectionManager.h"
#include "Selectable.h"

namespace audium {

void SelectionManager::deselectAll() {
    auto objects = selectedObjects;
    for (auto object : objects)
        object->setSelected(false);
    
    jassert(selectedObjects.size() == 0);
}

} // namespace audium
