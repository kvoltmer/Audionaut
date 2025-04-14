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

/**
 * @class SelectionManager
 * @brief Manages the selection of objects within the application.
 *
 * This class provides functionality to manage selected objects, handle
 * selection states, and perform operations like copying and pasting
 * selected objects.
 */
class SelectionManager
{
public:
    /**
     * @brief Default constructor.
     */
    SelectionManager() = default;

    /**
     * @brief Default destructor.
     */
    ~SelectionManager() = default;

    /**
     * @brief Selects or deselects an object.
     * @param object A shared pointer to the object to select or deselect.
     * @param bSelected True to select the object, false to deselect it.
     */
    void selectItem(std::shared_ptr<Selectable> object, bool bSelected)
    {
        auto it = std::find(selectedObjects.begin(), selectedObjects.end(), object);

        if (it != selectedObjects.end())
            selectedObjects.erase(it);

        if (bSelected)
            selectedObjects.push_back(object);
    }

    /**
     * @brief Checks if any object is currently selected.
     * @return True if at least one object is selected, false otherwise.
     */
    bool isSomethingSelected() const { return selectedObjects.size() > 0; }

    /**
     * @brief Gets the list of currently selected objects.
     * @return A const reference to the vector of selected objects.
     */
    const std::vector<std::shared_ptr<Selectable>>& getSelectedObjects() const { return selectedObjects; }

    /**
     * @brief Clears the selection, deselecting all objects.
     */
    void clear() { selectedObjects.clear(); }

    /**
     * @brief Deselects all currently selected objects.
     */
    void deselectAll();

    /**
     * @brief Gets the current selection context.
     * @return The type of the current selection context.
     */
    const SelectionContextType getSelectionContext() const;

    /**
     * @brief Copies the currently selected objects to the clipboard.
     */
    void copySelectedToClipboard();

    /**
     * @brief Checks if the clipboard contains parsable data.
     * @return True if the clipboard contains valid data, false otherwise.
     */
    bool canParseFromClipboard();

    /**
     * @brief Pastes objects from the clipboard into the application.
     * @param audiumEngine A shared pointer to the AudiumEngine instance.
     * @param duplicateAction True to duplicate the objects, false to paste them normally.
     */
    void pasteFromClipboard(std::shared_ptr<AudiumEngine> audiumEngine, bool duplicateAction);

private:
    /**
     * @brief Pastes playlist items from the clipboard.
     * @param input The JSON data from the clipboard.
     * @param audiumEngine A shared pointer to the AudiumEngine instance.
     * @param duplicateAction True to duplicate the items, false to paste them normally.
     */
    void pastePlayListItems(const json& input,
                            std::shared_ptr<AudiumEngine> audiumEngine,
                            bool duplicateAction);

    std::vector<std::shared_ptr<Selectable>> selectedObjects; ///< The list of currently selected objects.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectionManager)
};

} // namespace audium
