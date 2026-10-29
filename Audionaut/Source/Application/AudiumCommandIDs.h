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
        revertProject           = 0x300008,
        importAudio             = 0x300009,

        showAboutWindow         = 0x300024,
        checkForNewVersion      = 0x300025,
        
    
        showSettingsWindow      = 0x300030,

        clearRecentFiles        = 0x300049,
        
        playStop                = 0x300050,
        loopSelection           = 0x300052,
        
        createRegion            = 0x300060,
        autoEdit                = 0x300061,
        duplicate               = 0x300062,
        cleanupRegions          = 0x300063,
        splitRegion             = 0x300064,
        assembleSequential      = 0x300065,
        assembleRandom          = 0x300066,
        separateStems           = 0x300067,
        stretchClip             = 0x300068,

        enableSnapToGrid        = 0x300070,
        zoomIn                  = 0x300071,
        zoomOut                 = 0x300072,
        followTransport         = 0x300075,
        showFileBrowser         = 0x300076,
        toggleFullScreen        = 0x300077,
        pageLeft                = 0x300078,
        pageRight               = 0x300079,
        showProcessingLoad      = 0x30007a,
        showLoadSimulator       = 0x30007b,   // debug builds only
        
        createAudioTrack     = 0x300085,

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
