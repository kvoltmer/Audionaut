#include <catch2/catch_test_macros.hpp>

#include "Engine/AudioResourceContainer.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Components/WaveFormComponent.h"

static bool writeComponentImageToFile (const File& file, Component& comp, Rectangle<int> subArea)
{
    if (ImageFileFormat* format = ImageFileFormat::findImageFormatForFileExtension (file))
    {
        FileOutputStream out (file);

        if (out.openedOk())
            return format->writeImageToStream (comp.createComponentSnapshot (subArea), out);
    }

    return false;
}

TEST_CASE( "resource container", "[AudioResourceContainer]" ) {
    
    juce::MessageManager::getInstance(); // Force the MessageManager singleton to create an instance
    juce::MessageManagerLock mmLock(Thread::getCurrentThread());
    
    auto testFilesDirectory = std::string("../../../TestFiles/");
    
    auto container = std::shared_ptr<AudioResourceContainer>(new AudioResourceContainer());
    REQUIRE( container != nullptr );
    
    
    
    
    auto zoomHandler = std::shared_ptr<ZoomHandler>(new ZoomHandler(container));
    
    // setting up a scrollbar is odd
    auto scrollbar = std::shared_ptr<juce::ScrollBar>(new juce::ScrollBar(false));
    scrollbar->setRangeLimits(0, 1200);
    scrollbar->setCurrentRange(0, 1200);
    zoomHandler->setHorizontalScrollBar(scrollbar.get());
    
    auto waveFormComponent = std::shared_ptr<WaveFormComponent>(new WaveFormComponent(zoomHandler));
    waveFormComponent->setSize (1200, 400);
    // this is odd
    zoomHandler->setWidth(waveFormComponent->getWidth());
    
    
    auto inFile = File(testFilesDirectory + "silence-fade.aiff");
    auto audioresource = container->addAudioResource(juce::URL(inFile));
    waveFormComponent->setAudioResource(audioresource);
    
    while (!audioresource->isThumbnailFullyLoaded()) {
        Time::waitForMillisecondCounter(Time::getMillisecondCounter() + 10);
    }
    
    auto outFile = File(testFilesDirectory + "out.png");
    if (outFile.existsAsFile())
    {
        outFile.deleteFile();
    }
    writeComponentImageToFile(outFile, *waveFormComponent.get(), waveFormComponent->getBounds());

    waveFormComponent = nullptr;
    
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
    
    

}

