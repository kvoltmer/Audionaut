#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/AudioSources/VoiceSource.h"
#include "Engine/Export/AudioExporter.h"
#include "Engine/Export/ExportAudioConfig.h"
#include "Engine/PlayList/PlayListScheduler.h"

#include "Engine/PlayList/TransportLoop.h"
#include "Engine/Provider/TempoProvider.h"

#include "TestUtils.h"

using namespace audium;
using namespace juce;


SCENARIO("transport loop scenario", "[engine][transport][loop]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    
    GIVEN("a TransportLoop")
    {
        auto tempoProvider = std::make_shared<TempoProvider>(nullptr);
        auto transportLoop = std::make_unique<audium::TransportLoop>(nullptr,
                                                                     tempoProvider);
        transportLoop->setLoopActive(true);
        transportLoop->prepareToPlay(512, 44100.0);
        
        
        WHEN("processing the loop")
        {
            // loop between 0 and 1
            transportLoop->setLoopPositionRange(nullptr, {0.0, 1.0}, audium::seconds);
            auto transportPos = 0.0;
            auto delta = 0.1; // seconds
            
            for (auto i = 0; i < 1001; i++) {
                
                auto samples = static_cast<int>(44100.0 * delta);
                auto result = transportLoop->processLoop(transportPos, samples, audium::clocks);
                if (result.loopEvent) {
                    REQUIRE(result.numSamplesUntilLoop >= 0);
                    REQUIRE(result.numSamplesUntilLoop <= samples);
                }
                
                
                // fake transport
                auto inc = tempoProvider->secondsToClocks(0.1);
                
                transportPos += inc;
            }
            
            THEN("check on loop count")
            {
                REQUIRE(transportLoop->getLoopCount() == 100);
            }
        }
        
        WHEN("loop phase for position")
        {
            // loop between 2 and 3
            transportLoop->setLoopPositionRange(nullptr, {2.0, 3.0}, audium::clocks);
            
            THEN("check on loop phase for position")
            {
                auto phase = 0.0;
                // start < loop start and process 1.5 -> end up in the middle of the loop
                phase = transportLoop->getLoopPhaseForPosition(1.0, 1.5, audium::clocks);
                REQUIRE(static_cast<int>(phase) == 0);
                // hit the loop end -> should count as 1
                phase = transportLoop->getLoopPhaseForPosition(1.0, 2.0, audium::clocks);
                REQUIRE(static_cast<int>(phase) == 1);

                phase = transportLoop->getLoopPhaseForPosition(2.5, 0.0, audium::clocks);
                REQUIRE(static_cast<int>(phase) == 0);
                
                phase = transportLoop->getLoopPhaseForPosition(3.0, 0.0, audium::clocks);
                REQUIRE(static_cast<int>(phase) == 1);
                
            }
        }
    }
    
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

static void examineBouncedAudioFile(std::shared_ptr<audium::ExportAudioConfig> bounceConfig)
{
    AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    std::unique_ptr<AudioFormatReader> reader;
    reader.reset(formatManager.createReaderFor(bounceConfig->fileName));

    jassert(reader);
    
    AudioBuffer<float> buffer((int)reader->numChannels, (int)reader->lengthInSamples);
    
    auto success = reader->read(&buffer,
                                0,
                                (int)reader->lengthInSamples,
                                0,
                                true,
                                true);
    REQUIRE(success);
    
    for (auto i = 1; i < static_cast<int>(bounceConfig->lengthSeconds); i++) {
        auto samplePerPhase = 44100;
        for (auto s = 0; s < samplePerPhase; s++) {
            auto val0 = genSaw(s, samplePerPhase);
            auto val1 = buffer.getSample(0, s + (samplePerPhase * i));
            auto margin = 0.000001f;
            if (val0 != Catch::Approx(val1).margin(margin)) {
            }
            REQUIRE(val0 == Catch::Approx(val1).margin(margin));
        }
    }
}


SCENARIO("bounce loop scenario", "[engine][bounce][transport][loop]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    
    auto testFile = createSlowSawAudioFile();
    jassert(testFile.existsAsFile());
    std::cout << "Testfile: " << testFile.getFullPathName() << std::endl;
    
    GIVEN("generated audio file with loop")
    {
        auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();
        auto ok = store->open(testFile, nullptr);
        REQUIRE(ok);
        engine->getPlayListScheduler()->commitPlayListData();
        auto loop = engine->getPlayListScheduler()->getTransportLoop();
        loop->setLoopActive(true);
        loop->setLoopPositionRange(nullptr, {1.0, 2.0}, audium::seconds);
 
        auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
        bounceConfig->fileName = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/slow-saw-out.wav"));
        bounceConfig->sampleRate = 44100.0;
        bounceConfig->blockSize = 64;
        bounceConfig->lengthSeconds = 60.0;
        
        auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
        
        WHEN("bouncing session")
        {
            exporter->bounce();

            THEN("examine bounced audio file")
            {
                examineBouncedAudioFile(bounceConfig);
            }
        }
        WHEN("bouncing session with length to match loop end")
        {
            auto track = engine->getAudioTrackContainer()->getAudioTracks().front();
            auto item = track->getPlayListContainer()->getPlayListItem(0);
            item->setLength(2.0, audium::seconds);

            exporter->bounce();

            THEN("examine bounced audio file")
            {
                examineBouncedAudioFile(bounceConfig);
            }
        }
        WHEN("bouncing session with start and end to match loop")
        {
            auto track = engine->getAudioTrackContainer()->getAudioTracks().front();
            auto item = track->getPlayListContainer()->getPlayListItem(0);
            item->setAbsolutePosition(1.0, audium::seconds);
            item->getRegion()->setRegionData({1.0, 2.0}, audium::seconds);

            exporter->bounce();

            THEN("examine bounced audio file")
            {
                examineBouncedAudioFile(bounceConfig);
            }
        }
        
        
        if (bounceConfig->fileName.existsAsFile())
            bounceConfig->fileName.deleteFile();
        
        engine = nullptr;
    }
    
    
    if (testFile.existsAsFile())
        testFile.deleteFile();

    
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}


//==============================================================================
// Loop wraps with a source at another sample rate than the engine renders
// at. The clip chain then resamples, and a loop wrap is a seek through the
// resampler: whatever the seek does to the resampler's state (its
// anti-aliasing filter, its sub-sample phase) shows up at the loop
// boundary as a discontinuity in an otherwise continuous signal.
//
// The source is a sine whose frequency makes it periodic over the loop
// length, so a correct loop renders the plain, unbroken sine of the
// timeline. Two checks per wrap: no sample-to-sample step larger than the
// sine's own slope (a click), and the output after every wrap continues
// the sine fitted to the first, seek-free pass (timing and phase).

namespace {

struct LoopBounce {
    juce::AudioBuffer<float> buffer;
    double sampleRate = 0.0;
    double loopStart = 0.0;
    double loopLength = 0.0;
    double lengthSeconds = 0.0;

    int numWraps() const
    {
        return static_cast<int>(std::floor((lengthSeconds - loopStart) / loopLength));
    }

    /// The output sample the k-th wrap (k >= 1) lands on.
    int wrapSample(int k) const
    {
        return static_cast<int>(std::round((loopStart + k * loopLength) * sampleRate));
    }
};

LoopBounce bounceLoopedSine(const juce::File& sineFile, double processingRate,
                            juce::Range<double> loopRange, double lengthSeconds, int blockSize)
{
    auto engine = AudiumFactory::createAudiumEngine();
    REQUIRE(engine->getProjectFileStore()->open(sineFile, nullptr));

    auto scheduler = engine->getPlayListScheduler();
    scheduler->commitPlayListData();
    auto loop = scheduler->getTransportLoop();
    loop->setLoopActive(true);
    loop->setLoopPositionRange(nullptr, loopRange, audium::seconds);

    auto config = std::make_shared<ExportAudioConfig>();
    config->fileName = File(String(CURRENT_SOURCE_DIR) + "/TestFiles/loop-sine-out.wav");
    config->sampleRate = processingRate;
    config->blockSize = blockSize;
    config->numChannels = 1;
    config->bitDepth = 32;
    config->positionSeconds = 0.0;
    config->lengthSeconds = lengthSeconds;

    AudioExporter(*engine, config).bounce();

    LoopBounce result;
    result.buffer = audioFileToAudioBuffer(config->fileName);
    result.sampleRate = processingRate;
    result.loopStart = loopRange.getStart();
    result.loopLength = loopRange.getLength();
    result.lengthSeconds = lengthSeconds;

    config->fileName.deleteFile();
    engine = nullptr;
    return result;
}

/// Least-squares fit of a sine of known frequency over [from, to):
/// returns the reference r[n] = a sin(w n) + b cos(w n) for the whole
/// buffer length.
std::vector<float> fitSine(const juce::AudioBuffer<float>& buffer, double frequencyHz,
                           double sampleRate, int from, int to)
{
    const auto w = MathConstants<double>::twoPi * frequencyHz / sampleRate;
    double s = 0.0, c = 0.0;
    for (auto n = from; n < to; ++n) {
        s += buffer.getSample(0, n) * std::sin(w * n);
        c += buffer.getSample(0, n) * std::cos(w * n);
    }
    const auto a = 2.0 * s / (to - from);
    const auto b = 2.0 * c / (to - from);

    std::vector<float> reference(static_cast<size_t>(buffer.getNumSamples()));
    for (auto n = 0; n < buffer.getNumSamples(); ++n)
        reference[static_cast<size_t>(n)] = static_cast<float>(a * std::sin(w * n) + b * std::cos(w * n));
    return reference;
}

int nearestWrapDistance(const LoopBounce& bounce, int sample)
{
    auto distance = std::numeric_limits<int>::max();
    for (auto k = 1; k <= bounce.numWraps(); ++k)
        distance = std::min(distance, std::abs(sample - bounce.wrapSample(k)));
    return distance;
}

} // namespace

// Guards ClipResamplingSource's seek: juce::ResamplingAudioSource zeroed
// its anti-aliasing filter on every seek (a click at every wrap) and
// landed on a whole source sample after rounding to a device sample first
// (a phase error), while the scheduler seeks to the loop start although
// the voice restarts on a rounded output sample.
SCENARIO("looping a clip at another sample rate is seamless at the loop boundaries",
         "[engine][transport][loop][samplerate][resample]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    // 1.25 s loop, 1 kHz sine: 1250 cycles per loop, so the loop is
    // phase-continuous. The loop start sits on neither a sample nor a
    // block boundary at any rate.
    const auto frequencyHz = 1000.0;
    const auto loopRange = juce::Range<double>(0.7137, 0.7137 + 1.25);
    const auto lengthSeconds = 6.0;

    // the fit tolerates the resampler's interpolation ripple (~0.2 % at
    // 1 kHz); a resampler transient or a half-source-sample timing error
    // after the seek is an order of magnitude above it
    const auto tolerance = 0.01f;

    for (const auto fileRate : { 44100.0, 48000.0, 96000.0 })
    {
        for (const auto processingRate : { 44100.0, 48000.0 })
        {
            DYNAMIC_SECTION("a " << static_cast<int>(fileRate) << " Hz sine rendered at "
                            << static_cast<int>(processingRate) << " Hz")
            {
                auto sineFile = createSineAudioFile(frequencyHz, lengthSeconds, 0.0, fileRate);
                REQUIRE(sineFile.existsAsFile());

                for (const auto blockSize : { 512, 64 })
                {
                    DYNAMIC_SECTION("in " << blockSize << "-sample blocks")
                    {
                        auto bounce = bounceLoopedSine(sineFile, processingRate, loopRange, lengthSeconds, blockSize);
                        const auto& buffer = bounce.buffer;
                        const auto sr = bounce.sampleRate;
                        REQUIRE(bounce.numWraps() == 4);
                        REQUIRE(buffer.getNumSamples() == static_cast<int>(lengthSeconds * sr));

                        // fit the reference on the second half of the seek-free first pass
                        const auto fitFrom = static_cast<int>(std::round(0.5 * (bounce.loopStart + bounce.loopLength) * sr));
                        const auto reference = fitSine(buffer, frequencyHz, sr, fitFrom, bounce.wrapSample(1) - 1);

                        const auto worstDeviationIn = [&] (int from, int to)
                        {
                            auto worst = 0.0f;
                            for (auto n = from; n < to; ++n)
                                worst = std::max(worst, std::abs(buffer.getSample(0, n) - reference[static_cast<size_t>(n)]));
                            return worst;
                        };

                        THEN("the first pass matches its own fit")
                        {
                            REQUIRE(worstDeviationIn(fitFrom, bounce.wrapSample(1) - 1) < tolerance);
                        }

                        THEN("no sample-to-sample step exceeds the sine's own slope")
                        {
                            // the sine's steepest step between two samples
                            const auto maxSlope = 2.0 * std::sin(MathConstants<double>::pi * frequencyHz / sr);

                            auto worstStep = 0.0;
                            auto worstAt = 0;
                            for (auto n = fitFrom; n < buffer.getNumSamples(); ++n) {
                                const auto step = std::abs(buffer.getSample(0, n) - buffer.getSample(0, n - 1));
                                if (step > worstStep) {
                                    worstStep = step;
                                    worstAt = n;
                                }
                            }
                            INFO("worst step " << worstStep << " at sample " << worstAt << " ("
                                 << nearestWrapDistance(bounce, worstAt) << " samples from a loop wrap), sine slope "
                                 << maxSlope);
                            REQUIRE(worstStep <= maxSlope * 1.05);
                        }

                        THEN("after every wrap the output continues the sine of the first pass")
                        {
                            for (auto k = 1; k <= bounce.numWraps(); ++k)
                            {
                                const auto wrap = bounce.wrapSample(k);
                                const auto next = k < bounce.numWraps() ? bounce.wrapSample(k + 1) : buffer.getNumSamples();

                                // the seek itself: a resampler that resets its
                                // filter state rings here
                                const auto transient = worstDeviationIn(wrap - 2, wrap + 16);
                                // once settled: a seek that lands a fraction of a
                                // source sample off shows as a constant phase error
                                const auto settled = worstDeviationIn(wrap + 32, next - 2);

                                INFO("wrap " << k << " at sample " << wrap << ": deviation " << transient
                                     << " across the seek, " << settled << " once settled");
                                CHECK(transient < tolerance);
                                CHECK(settled < tolerance);
                            }
                        }
                    }
                }

                sineFile.deleteFile();
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
