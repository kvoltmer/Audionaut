#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Handlers/SnapToGridHandler.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "TestEngine.h"

using namespace Catch;
using namespace audium;

/// TODO: more testing 
TEST_CASE( "zoom handler", "[ZoomHandlerTests]" ) {
   
    TestEngine engine;
    auto playListScheduler = engine->getPlayListScheduler();
    auto snap = std::shared_ptr<SnapToGridHandler>(new SnapToGridHandler(playListScheduler));
    auto zoomHandler = std::shared_ptr<ZoomHandler>(new ZoomHandler(engine->getPlayListScheduler(), snap));
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

    // out of range factors are clamped to the zoom limits - a restored project
    // must never push the view past them
    zoomHandler->setZoomFactor(zoomHandler->maxZoomInFactor * 100.0);
    REQUIRE(zoomHandler->getZoomFactor() == zoomHandler->maxZoomInFactor);

    zoomHandler->setZoomFactor(zoomHandler->maxZoomOutFactor * 0.01);
    REQUIRE(zoomHandler->getZoomFactor() == zoomHandler->maxZoomOutFactor);

    zoomHandler->setZoomFactor(ZoomHandler::defaultZoomFactor);
    REQUIRE(zoomHandler->getZoomFactor() == ZoomHandler::defaultZoomFactor);


    zoomHandler->setZoomFactor(1.0);
    // 1 bar = 200 pixels
    // 1 second = 100 pixels (@120BPM)

    auto visibleRange = zoomHandler->getVisibleRange();
    
    
    
    auto visibleRangeSeconds = zoomHandler->getVisibleRangeInSeconds();

    auto regionPosition = juce::Range<double>(2.0, 3.0);

    
    snap = nullptr;
    playListScheduler = nullptr;
    zoomHandler = nullptr;
}
