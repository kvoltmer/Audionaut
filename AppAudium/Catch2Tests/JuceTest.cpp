#include <catch2/catch_test_macros.hpp>

#include "Engine/AudioResourceContainer.h"

TEST_CASE( "resource container", "[AudioResourceContainer]" ) {
    auto container = std::shared_ptr<AudioResourceContainer>(new AudioResourceContainer());
    REQUIRE( container != nullptr );
    

}

