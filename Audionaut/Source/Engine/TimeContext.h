//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

namespace audium
{

//==============================================================================
/**
 * @enum TimeContextType
 * @brief Represents different time contexts used in the application.
 *
 * The `TimeContextType` enum is used in various classes to indicate whether
 * time is represented in seconds or musical clocks.
 */
enum TimeContextType
{
    seconds = 0,    /**< Time in seconds. */
    clocks = 1,     /**< Time in clocks (musical time). */
};

} // namespace audium
