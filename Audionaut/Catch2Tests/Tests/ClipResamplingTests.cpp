//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <JuceHeader.h>

#include "Engine/AudioSources/ClipResamplingSource.h"
#include "Engine/AudioSources/ClipTransportSource.h"
#include "Engine/PlayList/ClipSpeed.h"

// The clip chain's resampler sizes its ring buffer in prepareToPlay, yet
// its ratio moves afterwards: the transport applies the file-rate
// correction once the chain is prepared, varispeed and tempo follow
// change it while the clip plays, and a standby lane gets its ratio at
// prime time. juce::ResamplingAudioSource grows the buffer inside
// getNextAudioBlock in that case - a heap allocation on the audio thread.
// These scenarios pin that the fork is sized for the whole range up front
// and that the transport hands it that range before preparing.

using namespace audium;

namespace {

/// Sample n holds the value n: the slope of whatever comes out of the
/// resampler is the ratio it ran at (its filter only delays a ramp).
juce::AudioBuffer<float> makeRamp (int numSamples)
{
    juce::AudioBuffer<float> ramp (1, numSamples);
    for (int i = 0; i < numSamples; ++i)
        ramp.setSample (0, i, static_cast<float> (i));
    return ramp;
}

juce::AudioBuffer<float> render (juce::AudioSource& source, int numBlocks, int blockSize)
{
    juce::AudioBuffer<float> out (1, numBlocks * blockSize);
    out.clear();

    for (int block = 0; block < numBlocks; ++block)
    {
        juce::AudioSourceChannelInfo info (&out, block * blockSize, blockSize);
        source.getNextAudioBlock (info);
    }

    return out;
}

/// The ramp's slope over the output, skipping the filter's settling.
double settledSlope (const juce::AudioBuffer<float>& out, int skip)
{
    const auto last = out.getNumSamples() - 1;
    return (out.getSample (0, last) - out.getSample (0, skip)) / static_cast<double> (last - skip);
}

} // namespace

SCENARIO ("the clip resampler never grows its buffer for a ratio within its bound", "[engine][resample][realtime]")
{
    constexpr int blockSize = 128;
    constexpr double sampleRate = 44100.0;
    constexpr double maximumRatio = 4.5;
    const auto ramp = makeRamp (1 << 17);

    GIVEN ("a resampler prepared at ratio 1 with a bound of 4.5")
    {
        juce::MemoryAudioSource input (const_cast<juce::AudioBuffer<float>&> (ramp), false);
        ClipResamplingSource resampler (&input, false, 1);
        resampler.setMaximumResamplingRatio (maximumRatio);
        resampler.prepareToPlay (blockSize, sampleRate);

        const auto preparedSize = resampler.getBufferSize();
        REQUIRE (preparedSize >= juce::roundToInt (blockSize * maximumRatio) + 11);
        REQUIRE (resampler.getBufferGrowths() == 0);

        WHEN ("the ratio jumps close to the bound and blocks are pulled")
        {
            constexpr double ratio = 4.35;
            constexpr int numBlocks = 20;
            resampler.setResamplingRatio (ratio);
            const auto out = render (resampler, numBlocks, blockSize);

            THEN ("the buffer stays as prepared and the output runs at the ratio")
            {
                REQUIRE (resampler.getBufferGrowths() == 0);
                REQUIRE (resampler.getBufferSize() == preparedSize);
                REQUIRE (settledSlope (out, 2 * blockSize) == Catch::Approx (ratio).margin (0.01));

                // the input advanced by the output's worth of source, plus
                // at most the look-ahead the resampler keeps buffered
                const auto consumed = static_cast<double> (input.getNextReadPosition());
                const auto expected = numBlocks * blockSize * ratio;
                CAPTURE (consumed, expected);
                REQUIRE (consumed >= expected);
                REQUIRE (consumed <= expected + preparedSize);
            }
        }

        WHEN ("a ratio beyond the bound comes through")
        {
            constexpr double ratio = 6.0;
            resampler.setResamplingRatio (ratio);
            const auto out = render (resampler, 10, blockSize);

            THEN ("the guarded fallback grows the buffer once and still plays")
            {
                REQUIRE (resampler.getBufferGrowths() == 1);
                REQUIRE (resampler.getBufferSize() > preparedSize);
                REQUIRE (settledSlope (out, 2 * blockSize) == Catch::Approx (ratio).margin (0.01));
            }
        }
    }
}

SCENARIO ("a clip at another sample rate plays its whole speed range without growing the resampler buffers", "[engine][resample][realtime][stretch]")
{
    constexpr int blockSize = 512;
    constexpr double deviceRate = 44100.0;
    constexpr double fileRate = 48000.0;
    constexpr double rateCorrection = fileRate / deviceRate;
    const auto ramp = makeRamp (1 << 18);

    GIVEN ("a prepared transport over a 48 kHz ramp on a 44.1 kHz device, with a standby lane")
    {
        juce::MemoryAudioSource live (const_cast<juce::AudioBuffer<float>&> (ramp), false);
        juce::MemoryAudioSource standby (const_cast<juce::AudioBuffer<float>&> (ramp), false);

        // wired as VoiceSource does: both lanes before prepareToPlay
        ClipTransportSource transport;
        transport.setSource (&live, 0, nullptr, fileRate, 1);
        transport.setStandbySource (&standby);
        transport.prepareToPlay (blockSize, deviceRate);

        // as the scheduler does when a clip is scheduled
        transport.resetClipGain();
        transport.clearFadeIn();
        transport.clearFadeOut();

        const auto* resampler = transport.getResamplerSource();
        const auto* standbyResampler = transport.getStandbyResampler();
        REQUIRE (resampler != nullptr);
        REQUIRE (standbyResampler != nullptr);

        const auto liveSize = resampler->getBufferSize();
        const auto standbySize = standbyResampler->getBufferSize();
        REQUIRE (resampler->getMaximumResamplingRatio() == Catch::Approx (rateCorrection * ClipSpeed::maxSpeedRatio));
        REQUIRE (standbyResampler->getMaximumResamplingRatio() == Catch::Approx (rateCorrection * ClipSpeed::maxSpeedRatio));

        transport.start();

        WHEN ("it plays as recorded and then at the fastest varispeed")
        {
            constexpr int numBlocks = 8;
            const auto unity = render (transport, numBlocks, blockSize);
            transport.setSpeedRatio (ClipSpeed::maxSpeedRatio);
            const auto fast = render (transport, numBlocks, blockSize);

            THEN ("no block grew the buffer, and the output follows both ratios")
            {
                REQUIRE (resampler->getBufferGrowths() == 0);
                REQUIRE (resampler->getBufferSize() == liveSize);
                REQUIRE (settledSlope (unity, blockSize) == Catch::Approx (rateCorrection).margin (0.01));
                REQUIRE (settledSlope (fast, blockSize) == Catch::Approx (rateCorrection * ClipSpeed::maxSpeedRatio).margin (0.01));
            }
        }

        WHEN ("the standby lane is primed for a wrap in Stretch mode")
        {
            transport.setStretchMode (StretchMode::Stretch);
            render (transport, 4, blockSize);   // primes the live stretcher

            constexpr int horizon = 12;
            for (int blocksLeft = horizon; blocksLeft >= 1; --blocksLeft)
            {
                transport.primeStandby (1.0, 1.0, blocksLeft);
                render (transport, 1, blockSize);
            }

            THEN ("the standby resampler served the prime without growing")
            {
                REQUIRE (transport.getStretchSource()->hasReadyStandby());
                REQUIRE (standbyResampler->getBufferGrowths() == 0);
                REQUIRE (standbyResampler->getBufferSize() == standbySize);
                REQUIRE (resampler->getBufferGrowths() == 0);
            }
        }
    }
}
