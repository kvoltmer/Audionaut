//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

namespace audium {

/**
 * @brief Action message indicating a change in tempo.
 */
const char* const tempoChanged = "tempo changed";

/**
 * @brief Action message indicating vertical scrolling in the arrangement view.
 */
const char* const scrolledVertically = "arrangement scrolled";

/**
 * @brief Action message to trigger a complete rebuild of the application state.
 */
const char* const rebuildAll = "rebuild all";

/**
 * @brief Action message to trigger an update of all components.
 */
const char* const updateAll = "update all";

/**
 * @brief Action message to trigger an update of the selected components.
 */
const char* const updateSelection = "update selection";

/**
 * @brief Action message to update the middle panel of the user interface.
 */
const char* const updateMiddlePanelAction = "update middle panel";

/**
 * @brief Action message to update the right panel of the user interface.
 */
const char* const updateRightPanelAction = "update right panel";

/**
 * @brief Action message to update the arrangement view.
 */
const char* const updateArrangementAction = "update arrangement";

} // namespace audium

