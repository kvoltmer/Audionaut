//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

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
    ogg
};

/** Every format, in the order the export dialog lists them. */
inline std::array<ExportFormat, 4> allExportFormats()
{
    return { ExportFormat::wav, ExportFormat::flac, ExportFormat::aiff, ExportFormat::ogg };
}

inline std::unique_ptr<juce::AudioFormat> createAudioFormat (ExportFormat format)
{
    switch (format) {
        case ExportFormat::flac: return std::make_unique<juce::FlacAudioFormat>();
        case ExportFormat::aiff: return std::make_unique<juce::AiffAudioFormat>();
        case ExportFormat::ogg:  return std::make_unique<juce::OggVorbisAudioFormat>();
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
        case ExportFormat::wav:  break;
    }
    return "WAV";
}

/** The name with its article, for messages: "a WAV file", "an AIFF file". */
inline juce::String aFileOf (ExportFormat format)
{
    auto name = formatName (format);
    return (format == ExportFormat::aiff || format == ExportFormat::ogg ? "an " : "a ") + name + " file";
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
    return std::nullopt;
}

/** The extensions every format can be written to, e.g. ".wav, .flac, .aiff or .ogg". */
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

/** Lossy formats have no bit depth; they take a quality instead. */
inline bool isLossy (ExportFormat format)
{
    return format == ExportFormat::ogg;
}

/** The most channels one file of the format can hold. FLAC streams stop
    at eight, and Vorbis defines channel layouts up to eight. JUCE's FLAC
    and Ogg writers leak their stream when they refuse a stream, so check
    this before creating a writer. */
inline int maxChannels (ExportFormat format)
{
    return format == ExportFormat::flac || format == ExportFormat::ogg
         ? 8 : std::numeric_limits<int>::max();
}

/** The bit depths a lossless format can write; empty for a lossy one. */
inline juce::Array<int> supportedBitDepths (ExportFormat format)
{
    if (isLossy (format))
        return {};
    return createAudioFormat (format)->getPossibleBitDepths();
}

/** The quality steps a lossy format offers, lowest first, as shown to the
    user (Ogg Vorbis: "64 kbps" ... "500 kbps", roughly - Vorbis encodes at
    a variable bit rate); empty for a lossless format. The index into this
    list is the quality an export takes. */
inline juce::StringArray qualityOptions (ExportFormat format)
{
    if (! isLossy (format))
        return {};
    return createAudioFormat (format)->getQualityOptions();
}

/** The quality a lossy export uses unless told otherwise: Ogg Vorbis
    quality 6, about 192 kbps. (JUCE's own default is the lowest step.) */
inline int defaultQuality (ExportFormat format)
{
    return format == ExportFormat::ogg ? 6 : -1;
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
