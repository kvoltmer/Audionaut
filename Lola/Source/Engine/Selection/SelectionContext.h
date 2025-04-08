//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

namespace audium
{

//==============================================================================
/**
    These enums indicate the selection context.
*/
enum SelectionContextType
{
    invalid_context = 0,
    play_list_item = 1,     /**<  */
    audio_region = 2,       /**< . */
    audio_track = 3,        /**< . */
    audio_channel = 4,      /**< . */
    sub_group = 5,          /**< . */
    
};

} // namespace audium
