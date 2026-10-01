//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/Export/Mp3AudioFormat.h"

#include <array>
#include <limits>
#include <optional>

namespace audium {

/** The file formats an export can write. The format follows the target
    file's extension wherever a file name is given (CLI, file chooser). */
enum class ExportFormat
{
    wav,
    flac,
    aiff,
    ogg,
    mp3
};

/** Every format, in the order the export dialog lists them. */
inline std::array<ExportFormat, 5> allExportFormats()
{
    return { ExportFormat::wav, ExportFormat::flac, ExportFormat::aiff, ExportFormat::ogg, ExportFormat::mp3 };
}

inline std::unique_ptr<juce::AudioFormat> createAudioFormat (ExportFormat format)
{
    switch (format) {
        case ExportFormat::flac: return std::make_unique<juce::FlacAudioFormat>();
        case ExportFormat::aiff: return std::make_unique<juce::AiffAudioFormat>();
        case ExportFormat::ogg:  return std::make_unique<juce::OggVorbisAudioFormat>();
        case ExportFormat::mp3:  return std::make_unique<Mp3AudioFormat>();
        case ExportFormat::wav:  break;
    }
    return std::make_unique<juce::WavAudioFormat>();
}

/** The extension including the dot, e.g. ".flac". */
inline juce::String fileExtension (ExportFormat format)
{
    switch (format) {
        case ExportFormat::flac: return ".flac";
        case ExportFormat::aiff: return ".aiff";
        case ExportFormat::ogg:  return ".ogg";
        case ExportFormat::mp3:  return ".mp3";
        case ExportFormat::wav:  break;
    }
    return ".wav";
}

/** The name shown to the user, e.g. "FLAC". */
inline juce::String formatName (ExportFormat format)
{
    switch (format) {
        case ExportFormat::flac: return "FLAC";
        case ExportFormat::aiff: return "AIFF";
        case ExportFormat::ogg:  return "Ogg Vorbis";
        case ExportFormat::mp3:  return "MP3";
        case ExportFormat::wav:  break;
    }
    return "WAV";
}

/** The name with its article, for messages: "a WAV file", "an MP3 file". */
inline juce::String aFileOf (ExportFormat format)
{
    auto name = formatName (format);
    const auto an = format == ExportFormat::aiff || format == ExportFormat::ogg || format == ExportFormat::mp3;
    return (an ? "an " : "a ") + name + " file";
}

/** The format a file's extension asks for, or nothing for an extension no
    export can write. */
inline std::optional<ExportFormat> exportFormatForFile (const juce::File& file)
{
    if (file.hasFileExtension (".wav"))
        return ExportFormat::wav;
    if (file.hasFileExtension (".flac"))
        return ExportFormat::flac;
    if (file.hasFileExtension (".aiff;.aif"))
        return ExportFormat::aiff;
    if (file.hasFileExtension (".ogg"))
        return ExportFormat::ogg;
    if (file.hasFileExtension (".mp3"))
        return ExportFormat::mp3;
    return std::nullopt;
}

/** The extensions every format can be written to, e.g. ".wav, .flac, ... or .mp3". */
inline juce::String exportExtensionList()
{
    juce::StringArray extensions;
    for (auto format : allExportFormats())
        extensions.add (fileExtension (format));
    return extensions.joinIntoString (", ", 0, extensions.size() - 1) + " or " + extensions[extensions.size() - 1];
}

/** A file chooser pattern for every format, e.g. "*.wav;*.flac;...". */
inline juce::String exportWildcard()
{
    juce::StringArray patterns;
    for (auto format : allExportFormats())
        patterns.add ("*" + fileExtension (format));
    patterns.add ("*.aif");
    return patterns.joinIntoString (";");
}

/** Lossy formats have no bit depth; they take a bit rate instead. */
inline bool isLossy (ExportFormat format)
{
    return format == ExportFormat::ogg || format == ExportFormat::mp3;
}

/** The most channels one file of the format can hold. FLAC streams stop
    at eight, Vorbis defines channel layouts up to eight, and MP3 is mono
    or stereo. JUCE's FLAC and Ogg writers leak their stream when they
    refuse a stream, so check this before creating a writer. */
inline int maxChannels (ExportFormat format)
{
    switch (format) {
        case ExportFormat::flac:
        case ExportFormat::ogg:  return 8;
        case ExportFormat::mp3:  return 2;
        case ExportFormat::wav:
        case ExportFormat::aiff: break;
    }
    return std::numeric_limits<int>::max();
}

/** The bit depths a lossless format can write; empty for a lossy one. */
inline juce::Array<int> supportedBitDepths (ExportFormat format)
{
    if (isLossy (format))
        return {};
    return createAudioFormat (format)->getPossibleBitDepths();
}

/** The bit rates a lossy format offers, lowest first, as shown to the user:
    Ogg Vorbis "64 kbps" ... "500 kbps" (roughly - Vorbis varies its bit
    rate with the material), MP3 "96 kbps" ... "320 kbps" (constant). Empty
    for a lossless format. The index into this list is the quality an
    export takes (ExportAudioConfig::quality). */
inline juce::StringArray qualityOptions (ExportFormat format)
{
    if (! isLossy (format))
        return {};
    return createAudioFormat (format)->getQualityOptions();
}

/** The kbps of each of qualityOptions(format), in the same order. */
inline juce::Array<int> bitRates (ExportFormat format)
{
    juce::Array<int> kbps;
    for (auto& option : qualityOptions (format))
        kbps.add (option.getIntValue());
    return kbps;
}

/** The quality index whose bit rate is the given kbps, or nothing when the
    format does not offer that rate. */
inline std::optional<int> qualityIndexForBitRate (ExportFormat format, int kbps)
{
    auto index = bitRates (format).indexOf (kbps);
    if (index < 0)
        return std::nullopt;
    return index;
}

/** The bit rate a lossy export uses unless told otherwise. */
constexpr int defaultBitRate = 192;

/** The quality index of defaultBitRate (both lossy formats offer it), or -1
    for a lossless format. (JUCE's own default for Ogg is the lowest step.) */
inline int defaultQuality (ExportFormat format)
{
    return qualityIndexForBitRate (format, defaultBitRate).value_or (-1);
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
