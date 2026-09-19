//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <vector>

#include "Engine/AudioSources/StretchAudioSource.h"
#include "Engine/AudioSources/VoiceSource.h"
#include "Engine/AudioSources/VoiceSourceContainer.h"
#include "Engine/Export/AudioExporter.h"
#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/PlayList/TransportLoop.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Resource/AudioResource.h"

#include "TestUtils.h"

// The loop wrap re-seeks every audible clip, and a Stretch-mode clip
// re-primes its stretcher on a seek: a window of look-ahead through Rubber
// Band inside the wrap block, several callbacks' worth per voice. The
// standby lane primes a second stretcher over the blocks before the wrap
// and swaps it in at the seek. These scenarios pin that the swapped-in
// lane renders exactly what an in-block prime would have, and that a
// looped bounce of a stretched clip goes through the standby every wrap.

using namespace audium;

namespace {

/// A 441 Hz tone from a table: two of these started fresh are identical,
/// so a standby fed from one matches a plain prime fed from the other.
class TableSine : public juce::AudioSource
{
public:
    void prepareToPlay (int, double) override
    {
        table.resize (100);
        for (size_t i = 0; i < table.size(); ++i)
            table[i] = static_cast<float> (std::sin (juce::MathConstants<double>::twoPi * static_cast<double> (i) / static_cast<double> (table.size())));
    }

    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        for (int i = 0; i < info.numSamples; ++i)
        {
            const auto value = table[position];
            position = (position + 1) % table.size();
            for (int channel = 0; channel < info.buffer->getNumChannels(); ++channel)
                info.buffer->setSample (channel, info.startSample + i, value);
        }
    }

private:
    std::vector<float> table;
    size_t position = 0;
};

juce::AudioBuffer<float> render (StretchAudioSource& node, int numBlocks, int blockSize)
{
    juce::AudioBuffer<float> out (1, numBlocks * blockSize);
    for (int block = 0; block < numBlocks; ++block)
        node.getNextAudioBlock (juce::AudioSourceChannelInfo (&out, block * blockSize, blockSize));
    return out;
}

float maxDifference (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b,
                     int from = 0, int to = -1)
{
    REQUIRE (a.getNumSamples() == b.getNumSamples());
    if (to < 0)
        to = a.getNumSamples();
    auto maxDiff = 0.0f;
    for (int i = from; i < to; ++i)
        maxDiff = std::max (maxDiff, std::abs (a.getSample (0, i) - b.getSample (0, i)));
    return maxDiff;
}

/// Rubber Band's real-time engine may render a stream a sample or two
/// earlier or later depending on how its input was chunked, so two
/// renders of the same material are compared at their best alignment
/// within a few samples: the lag found, and the mean absolute error there.
struct Alignment { int lag = 0; double meanError = 0.0; };

Alignment align (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b,
                 int from, int to, int maxLag = 4)
{
    Alignment best;
    best.meanError = 1.0e9;
    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        auto error = 0.0;
        auto count = 0;
        for (int i = from; i < to; ++i)
        {
            const auto j = i + lag;
            if (j < 0 || j >= b.getNumSamples())
                continue;
            error += std::abs (a.getSample (0, i) - b.getSample (0, j));
            ++count;
        }
        REQUIRE (count > 0);
        error /= count;
        if (error < best.meanError)
            best = { lag, error };
    }
    return best;
}

juce::AudioBuffer<float> readAudioFile (const juce::File& file)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    REQUIRE (reader != nullptr);
    juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels), static_cast<int> (reader->lengthInSamples));
    reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, true);
    return buffer;
}

} // namespace

SCENARIO ("a standby lane renders what an in-block prime renders", "[engine][stretch][loop][standby]")
{
    constexpr int blockSize = 128;
    constexpr double sampleRate = 44100.0;
    constexpr double ratio = 1.31;
    constexpr int compareBlocks = 40;

    GIVEN ("a stretch node with a standby lane, streaming")
    {
        TableSine live, standby;
        StretchAudioSource node (&live, 1);
        node.setStandbyInput (&standby);
        node.prepareToPlay (blockSize, sampleRate);
        node.setEnabled (true);
        node.setSpeedRatio (ratio);
        REQUIRE (node.hasStandbyLane());

        render (node, 10, blockSize);
        REQUIRE (node.getPrimeCount() == 1);

        WHEN ("the standby is primed over the blocks before a jump and adopted at it")
        {
            const juce::int64 key = 4711;
            constexpr int horizon = 30;
            for (int blocksLeft = horizon; blocksLeft >= 1; --blocksLeft)
            {
                node.primeStandby (key, ratio, blocksLeft);
                render (node, 1, blockSize);
            }
            REQUIRE (node.isStandbyPrimingFor (key));
            REQUIRE_FALSE (node.adoptStandby (key + 1));   // another position: no
            REQUIRE (node.adoptStandby (key));

            const auto out = render (node, compareBlocks, blockSize);

            THEN ("no prime ran in the block, and the output matches a fresh prime of the same material")
            {
                REQUIRE (node.getPrimeCount() == 1);
                REQUIRE (node.getStandbyAdoptions() == 1);
                REQUIRE (node.getUnderruns() == 0);

                TableSine referenceInput;
                StretchAudioSource reference (&referenceInput, 1);
                reference.prepareToPlay (blockSize, sampleRate);
                reference.setEnabled (true);
                reference.setSpeedRatio (ratio);
                const auto expected = render (reference, compareBlocks, blockSize);

                REQUIRE (out.getMagnitude (0, out.getNumSamples()) > 0.5f);
                const auto alignment = align (out, expected, 0, out.getNumSamples());
                CAPTURE (alignment.lag, alignment.meanError, maxDifference (out, expected));
                REQUIRE (std::abs (alignment.lag) <= 2);
                REQUIRE (alignment.meanError < 1.0e-3);
            }
        }

        WHEN ("a prime for one position is restarted for another")
        {
            node.primeStandby (1, ratio, 4);
            node.primeStandby (2, ratio, 2);
            THEN ("only the new key is tracked and adoption needs it complete")
            {
                REQUIRE_FALSE (node.isStandbyPrimingFor (1));
                REQUIRE (node.isStandbyPrimingFor (2));
                REQUIRE_FALSE (node.adoptStandby (1));
                node.primeStandby (2, ratio, 1);    // finishes it
                REQUIRE (node.adoptStandby (2));
                REQUIRE (node.getStandbyAdoptions() == 1);
            }
        }
    }
}

SCENARIO ("a looped stretched clip wraps through the standby lane", "[engine][loop][stretch][standby]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock (juce::Thread::getCurrentThread());

    auto testFile = createSlowSawAudioFile();
    REQUIRE (testFile.existsAsFile());

    const auto bounceFile = juce::File (juce::String (CURRENT_SOURCE_DIR) + "/TestFiles/loop-standby-out.wav");

    // a looped bounce of the stretched clip; returns the bounce and the
    // clip's stretch node counters
    struct Result { juce::AudioBuffer<float> audio; int primes = 0; int adoptions = 0; int loops = 0; };
    const auto bounce = [&] (bool standbyEnabled, int blockSize) {
        auto engine = AudiumFactory::createAudiumEngine();
        REQUIRE (engine->getProjectFileStore()->open (testFile, nullptr));
        auto item = engine->getAudioTrackContainer()->getAudioTrack (0)->getPlayListContainer()->getPlayListItem (0);
        REQUIRE (item != nullptr);
        item->setStretchMode (StretchMode::Stretch);
        item->setSpeedRatio (1.31);
        engine->getPlayListScheduler()->commitPlayListData();
        engine->getPlayListScheduler()->standbyPrimingEnabled.store (standbyEnabled);

        auto loop = engine->getPlayListScheduler()->getTransportLoop();
        loop->setLoopActive (true);
        loop->setLoopPositionRange (nullptr, {1.0, 2.0}, audium::seconds);

        auto config = std::make_shared<ExportAudioConfig>();
        config->fileName = bounceFile;
        config->sampleRate = 44100.0;
        config->blockSize = blockSize;
        config->numChannels = 1;
        config->lengthSeconds = 5.5;
        AudioExporter (*engine, config).bounce();

        Result result;
        result.audio = readAudioFile (bounceFile);
        result.loops = loop->getLoopCount();

        auto resources = item->getRegion()->getAudioResources();
        REQUIRE_FALSE (resources.empty());
        auto voices = engine->getAudioTrackContainer()->getVoiceSourceContainer()->getVoiceSourcesForResource (*resources.front());
        REQUIRE_FALSE (voices.empty());
        const auto* stretch = voices.front()->getClipTransportSource().getStretchSource();
        REQUIRE (stretch != nullptr);
        REQUIRE (stretch->hasStandbyLane());
        result.primes = stretch->getPrimeCount();
        result.adoptions = stretch->getStandbyAdoptions();

        bounceFile.deleteFile();
        return result;
    };

    for (const auto blockSize : { 128, 512 })
    {
        DYNAMIC_SECTION ("block " << blockSize)
        {
            const auto withStandby = bounce (true, blockSize);
            const auto without = bounce (false, blockSize);

            THEN ("every wrap adopted a primed standby instead of priming in the block")
            {
                REQUIRE (withStandby.loops >= 3);
                REQUIRE (withStandby.adoptions == withStandby.loops);
                REQUIRE (withStandby.primes == 1);             // the clip's start

                REQUIRE (without.adoptions == 0);
                REQUIRE (without.primes == 1 + without.loops);
            }

            THEN ("the bounce is the same audio either way")
            {
                const auto& a = withStandby.audio;
                const auto& b = without.audio;
                REQUIRE (a.getNumSamples() == b.getNumSamples());
                REQUIRE (a.getMagnitude (0, a.getNumSamples()) > 0.1f);

                // up to the first wrap nothing differs
                const auto firstWrap = static_cast<int> (2.0 * 44100.0) - blockSize;
                REQUIRE (maxDifference (a, b, 0, firstWrap) == 0.0f);

                // every pass after a wrap: the same material (see align;
                // the slow saw cannot resolve the lag, the error can)
                for (int pass = 2; pass < 5; ++pass)
                {
                    const auto from = static_cast<int> (pass * 44100.0) + blockSize;
                    const auto to = static_cast<int> ((pass + 1) * 44100.0) - blockSize;
                    const auto alignment = align (a, b, from, to);
                    const auto rms = b.getRMSLevel (0, from, to - from);
                    CAPTURE (pass, alignment.lag, alignment.meanError, rms);
                    REQUIRE (rms > 0.05f);
                    REQUIRE (alignment.meanError < 0.01 * rms);
                }
            }
        }
    }

    testFile.deleteFile();
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}
