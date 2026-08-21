#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/PlayList/ClipDynamics.h"
#include "Interface/Widgets/audium_AudioThumbnail.h"

TEST_CASE( "waveform fade envelope", "[WaveFormComponent][fade]" ) {

    SECTION("fade curve is the sqrt of the linear progress, clamped to 0..1") {
        REQUIRE(audium::ClipDynamics::fadeCurve(0.0)  == 0.0);
        REQUIRE(audium::ClipDynamics::fadeCurve(0.5)  == Catch::Approx(std::sqrt(0.5)));
        REQUIRE(audium::ClipDynamics::fadeCurve(1.0)  == 1.0);
        REQUIRE(audium::ClipDynamics::fadeCurve(-0.5) == 0.0);
        REQUIRE(audium::ClipDynamics::fadeCurve(2.0)  == 1.0);
    }

    SECTION("the curve exponent bends the fade") {
        // 1 = linear, > 1 = exponential; midpoint value = 0.5^p
        REQUIRE(audium::ClipDynamics::fadeCurve(0.5, 1.0) == Catch::Approx(0.5));
        REQUIRE(audium::ClipDynamics::fadeCurve(0.5, 2.0) == Catch::Approx(0.25));
        REQUIRE(audium::ClipDynamics::fadeCurve(0.0, 2.0) == 0.0);
        REQUIRE(audium::ClipDynamics::fadeCurve(1.0, 2.0) == 1.0);
    }

    SECTION("a default envelope is inactive") {
        audium::WaveformEnvelope envelope;
        REQUIRE_FALSE(envelope.isActive());
        REQUIRE(envelope.gainAt(0.0f) == 1.0f);
    }

    SECTION("the envelope follows the fade curve within the fade spans") {
        audium::WaveformEnvelope envelope;
        envelope.fadeInWidth  = 100.0f;
        envelope.fadeOutWidth = 50.0f;
        envelope.totalWidth   = 1000.0f;

        REQUIRE(envelope.isActive());

        // fade in: 0 at the clip start, sqrt curve up to 1 at fadeInWidth
        REQUIRE(envelope.gainAt(0.0f)  == 0.0f);
        REQUIRE(envelope.gainAt(50.0f) == Catch::Approx(std::sqrt(0.5)));

        // flat middle
        REQUIRE(envelope.gainAt(100.0f) == 1.0f);
        REQUIRE(envelope.gainAt(500.0f) == 1.0f);
        REQUIRE(envelope.gainAt(950.0f) == 1.0f);

        // fade out: sqrt curve down to 0 at the clip end
        REQUIRE(envelope.gainAt(975.0f)  == Catch::Approx(std::sqrt(0.5)));
        REQUIRE(envelope.gainAt(1000.0f) == 0.0f);
    }

    SECTION("ramp offsets shift the fade spans") {
        audium::WaveformEnvelope envelope;
        envelope.fadeInWidth      = 100.0f;
        envelope.fadeInStartWidth = 50.0f;   // silent head inside the clip
        envelope.fadeOutWidth     = 100.0f;
        envelope.fadeOutEndWidth  = 50.0f;   // silent tail inside the clip
        envelope.totalWidth       = 1000.0f;

        REQUIRE(envelope.gainAt(25.0f)  == 0.0f);
        REQUIRE(envelope.gainAt(75.0f)  == Catch::Approx(std::sqrt(0.5)));
        REQUIRE(envelope.gainAt(500.0f) == 1.0f);
        REQUIRE(envelope.gainAt(925.0f) == Catch::Approx(std::sqrt(0.5)));
        REQUIRE(envelope.gainAt(975.0f) == 0.0f);
    }

    SECTION("a ramp lying entirely outside the clip still tapers negative x") {
        // fade-in end at the clip start, start dragged outside: the ghost
        // waveform (drawn at x < 0 in clip-local space) must follow the ramp
        // even though the in-clip fade width is zero
        audium::WaveformEnvelope envelope;
        envelope.fadeInWidth      = 0.0f;
        envelope.fadeInStartWidth = -100.0f;
        envelope.fadeOutWidth     = 0.0f;
        envelope.fadeOutEndWidth  = -100.0f;
        envelope.totalWidth       = 1000.0f;

        // fade-in ramp over [-100, 0]
        REQUIRE(envelope.gainAt(-100.0f) == 0.0f);
        REQUIRE(envelope.gainAt(-50.0f)  == Catch::Approx(std::sqrt(0.5)));
        REQUIRE(envelope.gainAt(0.0f)    == 1.0f);

        // inside the clip untouched
        REQUIRE(envelope.gainAt(500.0f)  == 1.0f);
        REQUIRE(envelope.gainAt(1000.0f) == 1.0f);

        // fade-out ramp over [1000, 1100]
        REQUIRE(envelope.gainAt(1050.0f) == Catch::Approx(std::sqrt(0.5)));
        REQUIRE(envelope.gainAt(1100.0f) == 0.0f);
    }

    SECTION("the envelope follows the per-ramp curve exponents") {
        audium::WaveformEnvelope envelope;
        envelope.fadeInWidth  = 100.0f;
        envelope.fadeOutWidth = 100.0f;
        envelope.fadeInCurve  = 1.0f;  // linear
        envelope.fadeOutCurve = 2.0f;  // exponential
        envelope.totalWidth   = 1000.0f;

        REQUIRE(envelope.gainAt(50.0f)  == Catch::Approx(0.5));
        REQUIRE(envelope.gainAt(950.0f) == Catch::Approx(0.25));
    }

    SECTION("a ramp crossing the clip edge tapers both sides of it") {
        audium::WaveformEnvelope envelope;
        envelope.fadeInWidth      = 100.0f;
        envelope.fadeInStartWidth = -100.0f;
        envelope.totalWidth       = 1000.0f;

        REQUIRE(envelope.gainAt(-100.0f) == 0.0f);
        REQUIRE(envelope.gainAt(0.0f)    == Catch::Approx(std::sqrt(0.5)));
        REQUIRE(envelope.gainAt(100.0f)  == 1.0f);
    }
}

// TODO: update and activate this test

//#include "Engine/AudiumFactory.h"
//#include "Engine/AudioResourceContainer.h"
//#include "Interface/Handlers/ZoomHandler.h"
//#include "Interface/Components/WaveFormComponent.h"
//#include "Interface/AudiumLookAndFeel.h"
//
//static bool writeComponentImageToFile (const File& file, Component& comp, Rectangle<int> subArea)
//{
//    if (ImageFileFormat* format = ImageFileFormat::findImageFormatForFileExtension (file))
//    {
//        FileOutputStream out (file);
//
//        if (out.openedOk())
//            return format->writeImageToStream (comp.createComponentSnapshot (subArea), out);
//    }
//
//    return false;
//}
//
//TEST_CASE( "waveform component test", "[WaveFormComponent]" ) {
//
//    juce::MessageManager::getInstance(); // Force the MessageManager singleton to create an instance
//    juce::MessageManagerLock mmLock(Thread::getCurrentThread());
//
//    auto testFilesDirectory = std::string("../../../TestFiles/");
//
//    auto engine      = AudiumFactory::createAudiumEngine();
//    auto zoomHandler = std::shared_ptr<ZoomHandler>(new ZoomHandler(engine->getAudioResourceContainer(),
//                                                                    engine->getTransportSourceProvider()));
//
//    // setting up a scrollbar is odd
//    auto scrollbar = std::shared_ptr<juce::ScrollBar>(new juce::ScrollBar(false));
//    scrollbar->setRangeLimits(0, 1200);
//    scrollbar->setCurrentRange(0, 1200);
//    zoomHandler->setHorizontalScrollBar(scrollbar.get());
//
//    auto inFile = File(testFilesDirectory + "silence-fade.aiff");
//    auto audioresource = engine->getAudioResourceContainer()->addAudioResource(juce::URL(inFile));
//
//
//    auto waveFormComponent = std::shared_ptr<WaveFormComponent>(new WaveFormComponent(audioresource, engine->getPlayListContainer(), zoomHandler));
//    waveFormComponent->setSize (1200, 400);
//    // this is odd
//    zoomHandler->setWidth(waveFormComponent->getWidth());
//
//    // needed to find the colour
//    auto lookAndFeel = std::shared_ptr<AudiumLookAndFeel>(new AudiumLookAndFeel());
//    waveFormComponent->setLookAndFeel(lookAndFeel.get());
//
//    while (!audioresource->isThumbnailFullyLoaded()) {
//        Time::waitForMillisecondCounter(Time::getMillisecondCounter() + 10);
//    }
//
//    auto outFile = File(testFilesDirectory + "out.png");
//    if (outFile.existsAsFile())
//    {
//        outFile.deleteFile();
//    }
//
//    writeComponentImageToFile(outFile, *waveFormComponent.get(), waveFormComponent->getBounds());
//
//
//
//
//    waveFormComponent->setLookAndFeel(nullptr);
//
//
//    zoomHandler = nullptr;
//    waveFormComponent = nullptr;
//    engine = nullptr;
//
//    juce::DeletedAtShutdown::deleteAll();
//    juce::MessageManager::deleteInstance();
//
//
//
//}

