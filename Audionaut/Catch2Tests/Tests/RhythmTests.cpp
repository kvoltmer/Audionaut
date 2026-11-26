#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Analysis/Dsp/OnsetStrength.h"
#include "Engine/Analysis/Dsp/TempoEstimator.h"
#include "Engine/Analysis/Dsp/BeatTracker.h"

#include <atomic>
#include <cmath>

using namespace audium;

namespace {

constexpr double framesPerSecond = 22050.0 / 512.0;

// An onset strength envelope of impulses every `period` frames (fractional
// periods land on the nearest frame), over a low floor of noise.
std::vector<float> impulseEnvelope(double period, int numFrames, float impulse = 1.0f)
{
    juce::Random random(3);
    std::vector<float> envelope((size_t) numFrames);
    for (int i = 0; i < numFrames; ++i)
        envelope[(size_t) i] = 0.05f * random.nextFloat();
    for (double t = 0.0; t < numFrames; t += period)
        if (const auto frame = (size_t) std::lround(t); frame < envelope.size())
            envelope[frame] += impulse;
    return envelope;
}

// Fraction of expected frames with a found frame within the tolerance.
float recall(const std::vector<int>& expected, const std::vector<int>& found, int tolerance)
{
    if (expected.empty())
        return 1.0f;
    int hits = 0;
    for (auto e : expected)
        for (auto f : found)
            if (std::abs(e - f) <= tolerance)
            {
                ++hits;
                break;
            }
    return (float) hits / (float) expected.size();
}

} // namespace

SCENARIO("OnsetStrength rises where the spectrum gets louder", "[engine][analysis][rhythm]")
{
    const auto numBins = 1025;
    OnsetStrength strength(OnsetStrength::Parameters(), numBins, 22050.0);

    std::vector<float> quiet((size_t) numBins, 1.0e-3f);
    std::vector<float> loud((size_t) numBins, 1.0f);

    THEN("the first frame scores nothing, a jump up scores, a jump down does not")
    {
        CHECK(strength.next(quiet.data()) == 0.0f);
        CHECK(strength.next(quiet.data()) == Catch::Approx(0.0f).margin(1.0e-6f));
        CHECK(strength.next(loud.data()) > 1.0f);
        CHECK(strength.next(loud.data()) == Catch::Approx(0.0f).margin(1.0e-6f));
        CHECK(strength.next(quiet.data()) == 0.0f);
    }
}

SCENARIO("OnsetStrength::pickPeaks finds the impulses of an envelope", "[engine][analysis][rhythm]")
{
    GIVEN("impulses every 20 frames")
    {
        const auto envelope = impulseEnvelope(20.0, 400);
        const auto peaks = OnsetStrength::pickPeaks(envelope, framesPerSecond);

        THEN("every impulse is a peak and nothing else is")
        {
            std::vector<int> expected;
            for (int t = 0; t < 400; t += 20)
                expected.push_back(t);
            CHECK(peaks == expected);
        }
    }

    GIVEN("a flat envelope")
    {
        THEN("there are no peaks")
        {
            CHECK(OnsetStrength::pickPeaks(std::vector<float>(100, 0.5f), framesPerSecond).empty());
            CHECK(OnsetStrength::pickPeaks({}, framesPerSecond).empty());
        }
    }
}

SCENARIO("TempoEstimator reads the tempo off an impulse train", "[engine][analysis][rhythm]")
{
    const TempoEstimator estimator;

    GIVEN("impulses at 120 BPM, which is 21.5 frames apart")
    {
        const auto period = 60.0 * framesPerSecond / 120.0;
        const auto envelope = impulseEnvelope(period, 2000);

        THEN("the estimate is 120, to within a percent")
        {
            CHECK(estimator.estimate(envelope, framesPerSecond) == Catch::Approx(120.0f).epsilon(0.01));
        }
    }

    GIVEN("impulses at 90 BPM")
    {
        const auto envelope = impulseEnvelope(60.0 * framesPerSecond / 90.0, 2000);

        THEN("the prior does not pull it to 120 or 180")
        {
            CHECK(estimator.estimate(envelope, framesPerSecond) == Catch::Approx(90.0f).epsilon(0.02));
        }
    }

    GIVEN("silence")
    {
        THEN("there is no tempo")
        {
            CHECK(estimator.estimate(std::vector<float>(500, 0.0f), framesPerSecond) == 0.0f);
            CHECK(estimator.estimate({}, framesPerSecond) == 0.0f);
        }
    }
}

SCENARIO("BeatTracker lands its beats on the impulses", "[engine][analysis][rhythm]")
{
    const BeatTracker tracker;

    GIVEN("impulses at 120 BPM")
    {
        const auto period = 60.0 * framesPerSecond / 120.0;
        const auto numFrames = 2000;
        const auto envelope = impulseEnvelope(period, numFrames);
        const auto result = tracker.track(envelope, framesPerSecond, 120.0f);

        THEN("there is a beat on nearly every impulse, and the tempo comes back as 120")
        {
            std::vector<int> expected;
            for (double t = 0.0; t < numFrames; t += period)
                expected.push_back((int) std::lround(t));

            REQUIRE_FALSE(result.beatFrames.empty());
            CHECK(std::is_sorted(result.beatFrames.begin(), result.beatFrames.end()));
            CHECK(recall(expected, result.beatFrames, 1) >= 0.95f);
            CHECK(recall(result.beatFrames, expected, 1) >= 0.95f);
            CHECK(result.bpm == Catch::Approx(120.0f).epsilon(0.01));
        }
    }

    GIVEN("an abort flag already set")
    {
        const auto envelope = impulseEnvelope(21.5, 2000);
        std::atomic<bool> abort { true };

        THEN("the tracking is abandoned")
        {
            CHECK(tracker.track(envelope, framesPerSecond, 120.0f, BeatTracker::Parameters(), &abort).beatFrames.empty());
        }
    }

    GIVEN("no tempo or no envelope")
    {
        THEN("there are no beats")
        {
            CHECK(tracker.track(impulseEnvelope(21.5, 200), framesPerSecond, 0.0f).beatFrames.empty());
            CHECK(tracker.track({}, framesPerSecond, 120.0f).beatFrames.empty());
        }
    }
}
