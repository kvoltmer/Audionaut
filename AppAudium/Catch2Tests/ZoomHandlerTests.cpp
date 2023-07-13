#include <catch2/catch_test_macros.hpp>

#include "Engine/AudioResourceContainer.h"
#include "Interface/Handlers/ZoomHandler.h"

/// TODO: more testing 
TEST_CASE( "zoom handler", "[ZoomHandlerTests]" ) {
    auto container = std::shared_ptr<AudioResourceContainer>(new AudioResourceContainer());
    auto zoom = std::shared_ptr<ZoomHandler>(new ZoomHandler(container));
    REQUIRE( zoom != nullptr );
    
}


