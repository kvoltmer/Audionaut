#include <catch2/catch_test_macros.hpp>

#include "Engine/AudiumFactory.h"
#include "Engine/AudioResourceContainer.h"
#include "Interface/Handlers/ZoomHandler.h"

/// TODO: more testing 
TEST_CASE( "zoom handler", "[ZoomHandlerTests]" ) {
   
    juce::MessageManager::getInstance(); // Force the MessageManager singleton to create an instance
    juce::MessageManagerLock mmLock(Thread::getCurrentThread());
    
    auto engine      = AudiumFactory::createAudiumEngine();
    auto zoomHandler = std::shared_ptr<ZoomHandler>(new ZoomHandler(engine->getAudioResourceContainer(),
                                                                    engine->getTransportSourceProvider()));
    REQUIRE( zoomHandler != nullptr );
    
    zoomHandler = nullptr;
    engine = nullptr;
    
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
    
}


