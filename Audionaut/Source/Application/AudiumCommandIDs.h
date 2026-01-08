//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

/**
    A namespace to hold all the possible command IDs.
*/
namespace CommandIDs
{
    enum
    {
        newProject              = 0x300000,
        openProject             = 0x300003,
        defaultProject          = 0x300004,
        saveProject             = 0x300005,
        saveProjectAs           = 0x300006,
        bounceProject           = 0x300007,

        showAboutWindow         = 0x300024,
        checkForNewVersion      = 0x300025,
        enableNewVersionCheck   = 0x300026,
        
    
        showSettingsWindow      = 0x300030,

        closeWindow             = 0x300040,

        deleteSelectedItem      = 0x300046,
        clearRecentFiles        = 0x300049,
        
        playStop                = 0x300050,
        loopPlayList            = 0x300051,
        
        createRegion            = 0x300060,
        autoEdit                = 0x300061,
        duplicate               = 0x300062,
        cleanupRegions          = 0x300063,
        splitRegion             = 0x300064,

        enableSnapToGrid        = 0x300070,
        zoomIn                  = 0x300071,
        zoomOut                 = 0x300072,
        zoomNormal              = 0x300073,
        spaceBarDrag            = 0x300074,
        followTransport         = 0x300075,
        toggleEditArrangement   = 0x300076,
        toggleFullScreen        = 0x300077,
        pageLeft                = 0x300078,
        pageRight               = 0x300079,
        
        createAudioTrack     = 0x300085,
       
        exportSelectedItemsId   = 0xf836743,
        copyChansToNewTrackId   = 0xf836744,

        lastCommandIDEntry
    };
}


namespace CommandCategories
{
    static const char* const general       = "General";
    static const char* const editing       = "Editing";
    static const char* const create        = "Create";
    static const char* const view          = "View";
    static const char* const windows       = "Windows";
    static const char* const transport     = "Transport";
}
