/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2022 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 7 End-User License
   Agreement and JUCE Privacy Policy.

   End User License Agreement: www.juce.com/juce-7-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#pragma once

/**
    A namespace to hold all the possible command IDs.
*/
namespace CommandIDs
{
    enum
    {
        newProject              = 0x300000,
        open                    = 0x300003,
        closeDocument           = 0x300004,
        saveDocument            = 0x300005,
        saveDocumentAs          = 0x300006,

        showAboutWindow         = 0x300024,
        checkForNewVersion      = 0x300025,
        enableNewVersionCheck   = 0x300026,

        showProjectSettings     = 0x300030,

        closeWindow             = 0x300040,

        deleteSelectedItem      = 0x300046,

        playStop                = 0x300050,

        enableSnapToGrid        = 0x300070,
        zoomIn                  = 0x300071,
        zoomOut                 = 0x300072,
        zoomNormal              = 0x300073,
        spaceBarDrag            = 0x300074,

        lastCommandIDEntry
    };
}

namespace CommandCategories
{
    static const char* const general       = "General";
    static const char* const editing       = "Editing";
    static const char* const view          = "View";
    static const char* const windows       = "Windows";
    static const char* const transport     = "Transport";
}
