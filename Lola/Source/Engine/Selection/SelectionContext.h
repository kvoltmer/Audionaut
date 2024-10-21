/*
  ==============================================================================

    SelectionContext.h
    Created: 21 Oct 2024 11:40:06am
    Author:  Klaus Voltmer

  ==============================================================================
*/

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
