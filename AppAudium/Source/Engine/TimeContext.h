/*
  ==============================================================================

    TimeContext.h
    Created: 30 Dec 2023 9:50:13am
    Author:  Klaus Voltmer

  ==============================================================================
*/

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
