//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

namespace audium
{

//==============================================================================
/**
 * @enum SelectionContextType
 * @brief Represents the type of selection context in the application.
 *
 * This enum is used to identify the context in which a selection is made,
 * such as a playlist item, audio region, or audio track.
 */
enum SelectionContextType
{
    invalid_context = 0,    /**< Represents an invalid or uninitialized context. */
    play_list_item = 1,     /**< Represents a playlist item context. */
    audio_region = 2,       /**< Represents an audio region context. */
    audio_track = 3,        /**< Represents an audio track context. */
    audio_channel = 4,      /**< Represents an audio channel context. */
    sub_group = 5,          /**< Represents a subgroup context. */
};

} // namespace audium
