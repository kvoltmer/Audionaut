//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Mp3AudioFormat.h"

#include <lame.h>

namespace audium {

namespace {

const char* const mp3FormatName = "MP3 file";

/** A LAME encoder set up for one stream; closes itself. */
struct LameEncoder
{
    LameEncoder() : flags (lame_init()) {}
    ~LameEncoder() { if (flags != nullptr) lame_close (flags); }

    lame_global_flags* flags;

    JUCE_DECLARE_NON_COPYABLE (LameEncoder)
};

class Mp3Writer final : public juce::AudioFormatWriter
{
public:
    Mp3Writer (juce::OutputStream* out, double rate, unsigned int numChans,
               std::unique_ptr<LameEncoder> encoder_)
        : AudioFormatWriter (out, mp3FormatName, rate, numChans, 32),
          encoder (std::move (encoder_)),
          streamStart (out->getPosition())
    {
        // LAME takes floats, so take the render's samples as they are
        usesFloatingPointData = true;
    }

    ~Mp3Writer() override
    {
        // the frames LAME still holds
        ensureBufferSize (0);
        auto flushed = lame_encode_flush (encoder->flags, bufferData(), (int) mp3Buffer.getSize());
        if (flushed > 0)
            output->write (bufferData(), (size_t) flushed);

        // The LAME/Xing info frame goes over the placeholder LAME wrote as
        // the first frame: players read the exact length (and the encoder
        // delay, for gapless playback) from it.
        auto tagSize = lame_get_lametag_frame (encoder->flags, bufferData(), mp3Buffer.getSize());
        if (tagSize > 0 && tagSize <= mp3Buffer.getSize()) {
            auto end = output->getPosition();
            if (output->setPosition (streamStart)) {
                output->write (bufferData(), tagSize);
                output->setPosition (end);
            }
        }

        output->flush();
    }

    bool write (const int** samplesToWrite, int numSamples) override
    {
        if (numSamples <= 0)
            return true;

        // usesFloatingPointData: these are float channels
        auto left = reinterpret_cast<const float*> (samplesToWrite[0]);
        auto right = numChannels > 1 && samplesToWrite[1] != nullptr
                   ? reinterpret_cast<const float*> (samplesToWrite[1])
                   : left;

        ensureBufferSize (numSamples);
        auto encoded = lame_encode_buffer_ieee_float (encoder->flags, left, right, numSamples,
                                                      bufferData(), (int) mp3Buffer.getSize());
        if (encoded < 0)
            return false;

        return encoded == 0 || output->write (bufferData(), (size_t) encoded);
    }

private:
    /** LAME's own worst case for the bytes one call produces. */
    void ensureBufferSize (int numSamples)
    {
        auto needed = (size_t) (1.25 * numSamples) + 7200;
        if (mp3Buffer.getSize() < needed)
            mp3Buffer.setSize (needed);
    }

    unsigned char* bufferData() { return static_cast<unsigned char*> (mp3Buffer.getData()); }

    std::unique_ptr<LameEncoder> encoder;
    juce::int64 streamStart;
    juce::MemoryBlock mp3Buffer;   // grows to the largest block written so far

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Mp3Writer)
};

} // namespace

Mp3AudioFormat::Mp3AudioFormat() : AudioFormat (mp3FormatName, ".mp3") {}

juce::Array<int> Mp3AudioFormat::getPossibleSampleRates()
{
    return { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 };
}

juce::Array<int> Mp3AudioFormat::getPossibleBitDepths()   { return {}; }
bool Mp3AudioFormat::canDoStereo()                         { return true; }
bool Mp3AudioFormat::canDoMono()                           { return true; }
bool Mp3AudioFormat::isCompressed()                        { return true; }

juce::Array<int> Mp3AudioFormat::getBitRates()
{
    return { 96, 128, 160, 192, 256, 320 };
}

juce::StringArray Mp3AudioFormat::getQualityOptions()
{
    juce::StringArray options;
    for (auto kbps : getBitRates())
        options.add (juce::String (kbps) + " kbps");
    return options;
}

juce::AudioFormatReader* Mp3AudioFormat::createReaderFor (juce::InputStream* sourceStream,
                                                          bool deleteStreamIfOpeningFails)
{
    // writing only: juce::MP3AudioFormat reads MP3
    if (deleteStreamIfOpeningFails)
        delete sourceStream;
    return nullptr;
}

std::unique_ptr<juce::AudioFormatWriter> Mp3AudioFormat::createWriterFor (std::unique_ptr<juce::OutputStream>& streamToWriteTo,
                                                                          const juce::AudioFormatWriterOptions& options)
{
    const auto numChannels = options.getNumChannels();
    const auto bitRates = getBitRates();
    const auto qualityIndex = options.getQualityOptionIndex();

    if (streamToWriteTo == nullptr
        || numChannels < 1 || numChannels > 2
        || ! juce::isPositiveAndBelow (qualityIndex, bitRates.size())
        || options.getSampleRate() <= 0)
        return nullptr;

    // set the encoder up before taking the stream, so a refusal leaves it
    // with the caller
    auto encoder = std::make_unique<LameEncoder>();
    if (encoder->flags == nullptr)
        return nullptr;

    auto* flags = encoder->flags;
    lame_set_in_samplerate (flags, juce::roundToInt (options.getSampleRate()));
    lame_set_num_channels (flags, numChannels);
    lame_set_mode (flags, numChannels == 1 ? MONO : JOINT_STEREO);
    lame_set_VBR (flags, vbr_off);
    lame_set_brate (flags, bitRates[qualityIndex]);
    lame_set_quality (flags, 2);        // LAME's "high quality" algorithm choice
    lame_set_bWriteVbrTag (flags, 1);   // reserve the info frame the writer fills in on close

    if (lame_init_params (flags) < 0)
        return nullptr;

    return std::make_unique<Mp3Writer> (std::exchange (streamToWriteTo, {}).release(),
                                        options.getSampleRate(), (unsigned int) numChannels,
                                        std::move (encoder));
}

} // namespace audium
