#include <catch2/catch_test_macros.hpp>

#include "Engine/AudiumFactory.h"
#include "Engine/AudioResourceContainer.h"
#include "Interface/Handlers/ZoomHandler.h"

/// TODO: more testing 
TEST_CASE( "zoom handler", "[ZoomHandlerTests]" ) {
    
    auto engine      = AudiumFactory::createAudiumEngine();
    auto zoomHandler = std::shared_ptr<ZoomHandler>(new ZoomHandler(engine->getAudioResourceContainer(),
                                                                    engine->getTransportSourceProvider()));
    REQUIRE( zoomHandler != nullptr );
    
}


