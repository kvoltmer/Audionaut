//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

namespace audium
{

//==============================================================================
/**
    These enums are used in various classes to indicate the time context.
*/
enum TimeContextType
{
    seconds = 0,    /**< time in seconds. */
    clocks = 1,     /**< time in clocks (musical time). */
};

} // namespace audium
