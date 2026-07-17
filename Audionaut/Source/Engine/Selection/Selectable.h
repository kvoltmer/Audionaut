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

/**
 * @class Selectable
 * @brief Represents an object that can be selected and managed by a SelectionManager.
 *
 * This class provides functionality to manage the selection state of an object
 * and integrates with a SelectionManager to handle selection logic.
 */
class Selectable : public std::enable_shared_from_this<Selectable>
{
public:
    /**
     * @brief Constructs a Selectable object.
     * @param selectionManager A shared pointer to the SelectionManager managing this object.
     */
    Selectable(std::shared_ptr<SelectionManager> selectionManager_) :
        selectionManager(selectionManager_)
    {
    }
    
    /**
     * @brief Virtual destructor for the Selectable class.
     */
    virtual ~Selectable() = default;
    
    /**
     * @brief Sets the selection state of the object.
     * @param bSelected True to select the object, false to deselect it.
     */
    virtual void setSelected(bool bSelected) {
        if (selected != bSelected) {
            selected = bSelected;
            jassert(selectionManager);
            selectionManager->selectItem(getSharedPtr(), bSelected);
        }
    }
        
    /**
     * @brief Checks if the object is currently selected.
     * @return True if the object is selected, false otherwise.
     */
    virtual bool isSelected() const { return selected; }
    
    /**
     * @brief Gets a shared pointer to this object.
     * @return A shared pointer to this object.
     */
    std::shared_ptr<Selectable> getSharedPtr() { return shared_from_this(); }

    /**
     * @brief Gets a const shared pointer to this object.
     * @return A const shared pointer to this object.
     */
    std::shared_ptr<const Selectable> getSharedPtr() const { return shared_from_this(); }
    
    /**
     * @brief Cleans up resources or performs any necessary cleanup logic.
     * This method can be overridden by derived classes.
     */
    virtual void cleanup() {}
    
private:
    bool selected = false; ///< Indicates whether the object is selected.
        
    std::shared_ptr<SelectionManager> selectionManager; ///< The SelectionManager managing this object.
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Selectable)
};

} // namespace audium
