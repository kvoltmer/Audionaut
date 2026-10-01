//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include <limits>
#include <optional>

namespace audium {

/** The file formats an export can write. The format follows the target
    file's extension wherever a file name is given (CLI, file chooser). */
enum class ExportFormat
{
    wav,
    flac
};

inline std::unique_ptr<juce::AudioFormat> createAudioFormat (ExportFormat format)
{
    switch (format) {
        case ExportFormat::flac: return std::make_unique<juce::FlacAudioFormat>();
        case ExportFormat::wav:  break;
    }
    return std::make_unique<juce::WavAudioFormat>();
}

/** The extension including the dot, e.g. ".flac". */
inline juce::String fileExtension (ExportFormat format)
{
    return format == ExportFormat::flac ? ".flac" : ".wav";
}

/** The name shown to the user, e.g. "FLAC". */
inline juce::String formatName (ExportFormat format)
{
    return format == ExportFormat::flac ? "FLAC" : "WAV";
}

/** The format a file's extension asks for, or nothing for an extension no
    export can write. */
inline std::optional<ExportFormat> exportFormatForFile (const juce::File& file)
{
    if (file.hasFileExtension (".wav"))
        return ExportFormat::wav;
    if (file.hasFileExtension (".flac"))
        return ExportFormat::flac;
    return std::nullopt;
}

/** The most channels one file of the format can hold. FLAC streams stop
    at eight; JUCE's FLAC writer leaks its stream when it refuses more, so
    check this before creating a writer. */
inline int maxChannels (ExportFormat format)
{
    return format == ExportFormat::flac ? 8 : std::numeric_limits<int>::max();
}

inline juce::Array<int> supportedBitDepths (ExportFormat format)
{
    return createAudioFormat (format)->getPossibleBitDepths();
}

/** The deepest bit depth the format can hold that does not exceed the given
    one (e.g. a 32-bit float source exported as FLAC becomes 24 bits). Only
    for depths derived from the source material - a depth the user asked for
    is either written as asked or refused. */
inline int closestSupportedBitDepth (ExportFormat format, int bitDepth)
{
    auto depths = supportedBitDepths (format);
    if (depths.isEmpty() || depths.contains (bitDepth))
        return bitDepth;

    auto best = depths.getFirst();
    for (auto depth : depths)
        if (depth <= bitDepth && depth > best)
            best = depth;
    return best;
}

} // namespace audium
