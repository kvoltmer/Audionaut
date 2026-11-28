#include <catch2/catch_test_macros.hpp>

#include <JuceHeader.h>

using namespace juce;

// JUCE's FLAC reader seeks to a position rounded down to 512 samples, and the
// decoder then hands over the rest of the frame that position lies in. With a
// block size that is no multiple of 512 that rest can end before the position
// that was asked for, and the reader used to return silence for the whole read
// (patched in the JUCE fork the submodule points at). A clip that starts
// mid-file, a loop wrap or a jump during playback each seek once.
SCENARIO("a FLAC reader returns the file's samples after a seek", "[engine][flac][seek]")
{
    const auto file = File(CURRENT_SOURCE_DIR).getChildFile("TestFiles/blocks-1152.flac");

    AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    GIVEN("a file with 1152-sample blocks, read once straight through")
    {
        std::unique_ptr<AudioFormatReader> straight (formatManager.createReaderFor(file));
        REQUIRE(straight != nullptr);

        const auto length = (int) straight->lengthInSamples;
        AudioBuffer<float> expected ((int) straight->numChannels, length);
        REQUIRE(straight->read(&expected, 0, length, 0, true, true));
        REQUIRE(expected.getMagnitude(0, length) > 0.1f);

        WHEN("a second reader jumps about the file")
        {
            std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor(file));
            REQUIRE(reader != nullptr);

            constexpr auto readSize = 2048;
            AudioBuffer<float> got (expected.getNumChannels(), readSize);
            Random random (182);

            auto wrongReads = 0;
            auto firstWrongStart = -1;

            for (auto i = 0; i < 2000; i++) {
                const auto start = random.nextInt(length - readSize);
                REQUIRE(reader->read(&got, 0, readSize, start, true, true));

                auto wrong = false;
                for (auto channel = 0; channel < got.getNumChannels() && ! wrong; channel++)
                    for (auto s = 0; s < readSize && ! wrong; s++)
                        wrong = got.getSample(channel, s) != expected.getSample(channel, start + s);

                if (wrong) {
                    wrongReads++;
                    if (firstWrongStart < 0)
                        firstWrongStart = start;
                }
            }

            THEN("every read holds what the straight read found there")
            {
                INFO(wrongReads << " of 2000 reads differ, the first from sample " << firstWrongStart);
                REQUIRE(wrongReads == 0);
            }
        }
    }
}
