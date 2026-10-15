//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

namespace audium {

/**
 * @class Mp3AudioFormat
 * @brief Writes MP3 files with the LAME encoder (ThirdParty/lame).
 *
 * JUCE can read MP3 (juce::MP3AudioFormat) but not write it; its
 * LAMEEncoderAudioFormat runs an external lame executable, which the
 * sandboxed app cannot do. This format compiles LAME in instead. Writing
 * only: createReaderFor() returns nothing, so read MP3 files with JUCE's
 * own format.
 *
 * The quality options are constant bit rates ("96 kbps" ... "320 kbps");
 * the option index picks one. One or two channels; LAME resamples a rate
 * above 48 kHz, the highest MP3 has, down by itself.
 */
class Mp3AudioFormat final : public juce::AudioFormat
{
public:
    Mp3AudioFormat();

    juce::Array<int> getPossibleSampleRates() override;
    juce::Array<int> getPossibleBitDepths() override;
    bool canDoStereo() override;
    bool canDoMono() override;
    bool isCompressed() override;
    juce::StringArray getQualityOptions() override;

    juce::AudioFormatReader* createReaderFor (juce::InputStream*, bool deleteStreamIfOpeningFails) override;

    /** Fails - leaving the stream with the caller - for more than two
        channels, a quality index outside getQualityOptions(), or a sample
        rate LAME refuses. */
    std::unique_ptr<juce::AudioFormatWriter> createWriterFor (std::unique_ptr<juce::OutputStream>& streamToWriteTo,
                                                              const juce::AudioFormatWriterOptions& options) override;
    using AudioFormat::createWriterFor;

    /** The bit rates behind getQualityOptions(), in kbps. */
    static juce::Array<int> getBitRates();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Mp3AudioFormat)
};

} // namespace audium
