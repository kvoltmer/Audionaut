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

