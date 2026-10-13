//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/PlayList/PlayListItem.h"

namespace audium {

/**
 * @struct ExportAudioConfig
 * @brief Configuration settings for exporting audio.
 *
 * This structure contains parameters for audio export, such as bit depth,
 * sample rate, block size, and file name. It also includes progress tracking
 * information for the export process.
 */
struct ExportAudioConfig {

    bool userCanceled = false;       ///< A helper boolean to cancel the export
    int bitDepth = 24;               ///< The bit depth of the exported audio (e.g., 16, 24, 32).
    double sampleRate = 44100;       ///< The sample rate of the exported audio in Hz.
    int blockSize = 1024;            ///< The block size used during the export process.
    int numChannels = 2;             ///< The number of audio channels (e.g., 1 for mono, 2 for stereo).
    bool multiMono = false;          ///< Whether to export as multiple mono files.
    double positionSeconds = 0.0;    ///< The starting position for the export in seconds.
    double lengthSeconds = -1.0;     ///< The export length in seconds.
    juce::File fileName;             ///< The file to which the audio will be exported.
    
    std::shared_ptr<audium::PlayListItem> playListItem = nullptr;

    double progress = 0.0;           ///< The progress of the export process (0.0 to 1.0).
    std::string progressMessage;     ///< A message describing the current progress state.

    /** Why a bounce returned false without having been cancelled. */
    enum class Failure {
        none,               ///< Nothing went wrong (or the user cancelled).
        unsupportedFormat,  ///< The writer rejects the bit depth or channel count.
        cannotOpenOutput,   ///< The output file could not be opened for writing.
        writeFailed         ///< Writing or finalising the file failed.
    };
    Failure failure = Failure::none; ///< Set by the exporter when it returns false.
    juce::String error;              ///< Human-readable reason for the failure; empty otherwise.
};

} // namespace audium
