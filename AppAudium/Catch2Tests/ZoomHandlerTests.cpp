#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/AudioResourceContainer.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Engine/PlayList/PlayListScheduler.h"

using namespace Catch;

/// TODO: more testing 
TEST_CASE( "zoom handler", "[ZoomHandlerTests]" ) {
   
    juce::MessageManager::getInstance(); // Force the MessageManager singleton to create an instance
    juce::MessageManagerLock mmLock(Thread::getCurrentThread());
    
    auto engine      = AudiumFactory::createAudiumEngine();
    auto zoomHandler = std::shared_ptr<ZoomHandler>(new ZoomHandler(engine->getPlayListScheduler()));
    REQUIRE( zoomHandler != nullptr );
    

    std::vector<double> zoomValues = {1.0, 0.5, 2.0, zoomHandler->maxZoomInFactor, zoomHandler->maxZoomOutFactor };
    
    for (auto zoom : zoomValues)
    {
        zoomHandler->setZoomFactor(zoom);
        const auto seconds = 10.0;
        const auto x = zoomHandler->secondsToX(seconds);
        const auto secondsResult = zoomHandler->xToSeconds(x);
        REQUIRE_THAT(secondsResult, Catch::Matchers::WithinAbs(seconds, 0.0));
    }
    
    
    
//    double secondsToXWithOffset (const double time) const;
//    double xToSecondsWithOffset (const double x) const;
//
//    double barsToX (const double bars) const;
//    double xToBars (const double x) const;
//
//    double clocksToX (const double clocks) const;
//    double xToClocks (const double x) const;

    
    
    zoomHandler->setZoomFactor(1.0);
    auto scrollbar = std::make_unique<juce::ScrollBar>(false);
    zoomHandler->setHorizontalScrollBar(scrollbar.get());
    
    // 1 bar = 200 pixels
    // 1 second = 100 pixels (@120BPM)
    
    // Set the limit to 1000 pixels = 10 seconds @120BPM
    scrollbar->setRangeLimits(juce::Range<double>(0.0, 1000.0));
    // Changes the position of the scrollbar's 'thumb'.
    scrollbar->setCurrentRange(juce::Range<double>(100.0, 200.0));
    
    
    auto visibleRange = zoomHandler->getVisibleRange();
    
    
    
    auto visibleRangeSeconds = zoomHandler->getVisibleRangeInSeconds();

    auto regionPosition = juce::Range<double>(2.0, 3.0);
    
    auto regionRangeSeconds = zoomHandler->getPlayListScheduler()->clocksToSeconds(regionPosition);
    
    
    zoomHandler = nullptr;
    engine = nullptr;
    
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
    
}


//auto visibleRange = zoomHandler->getVisibleRange();
//
//// adjust the drawing area
////        thumbArea.setX(static_cast<int>(visibleRange.getStart()));
////        thumbArea.setWidth(static_cast<int>(visibleRange.getLength()));
////
//auto rangeInSeconds = zoomHandler->getVisibleRangeInSeconds();
////
////        audioResource->getThumbnail().drawChannels (g, thumbArea,
////                                                    rangeInSeconds.getStart(), rangeInSeconds.getEnd(), 1.0f);
//
//
////const auto startSecond = zoomHandler->getPlayListScheduler()->clocksToSeconds(audioRegion->position.getStart());
////const auto endSecond = zoomHandler->getPlayListScheduler()->clocksToSeconds(audioR egion->position.getEnd());
//auto rangeSeconds = zoomHandler->getPlayListScheduler()->clocksToSeconds(audioRegion->position);
//auto start = rangeSeconds.getStart();
//auto end = rangeSeconds.getEnd();
//thumb->drawChannels (g, thumbArea, start, end, 1.0f);
//
//std::cout << rangeInSeconds.getStart() << " " << rangeSeconds.getEnd() << " " << start << " " << end << std::endl;
