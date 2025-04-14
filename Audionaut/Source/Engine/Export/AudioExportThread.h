//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"

namespace audium {

/**
 * @class AudioExportThread
 * @brief Handles the audio export process in a separate thread with progress tracking.
 *
 * This class is responsible for exporting audio to a file using the provided
 * configuration. It runs the export process in a background thread and displays
 * a progress window to the user.
 */
class AudioExportThread : public juce::ThreadWithProgressWindow
{
public:
    /**
     * @brief Constructs an AudioExportThread instance.
     * @param audiumEngine_ Reference to the AudiumEngine instance.
     * @param config_ Reference to the ExportAudioConfig containing export settings.
     */
    AudioExportThread(AudiumEngine &audiumEngine_,
                      ExportAudioConfig &config_) :
        juce::ThreadWithProgressWindow ("exporting...", true, true),
        audiumEngine(audiumEngine_),
        config(config_)
    {
    }
    
    /**
     * @brief The main thread execution logic.
     *
     * This method is called when the thread starts and performs the audio export process.
     */
    void run()
    {
        bounceToFile(config);
    }
    
    /**
     * @brief Exports audio to a file based on the provided configuration.
     * @param config Reference to the ExportAudioConfig containing export settings.
     */
    void bounceToFile(ExportAudioConfig &config);
    
private:
    AudiumEngine &audiumEngine; ///< Reference to the AudiumEngine instance.
    ExportAudioConfig &config;  ///< Reference to the export configuration.
};

} // namespace audium

