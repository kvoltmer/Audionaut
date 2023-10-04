#include <catch2/catch_test_macros.hpp>

#include "Engine/AudiumFactory.h"
#include "Engine/AudioResourceContainer.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Components/WaveFormComponent.h"
#include "Interface/AudiumLookAndFeel.h"

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

TEST_CASE( "waveform component test", "[WaveFormComponent]" ) {
    
    juce::MessageManager::getInstance(); // Force the MessageManager singleton to create an instance
    juce::MessageManagerLock mmLock(Thread::getCurrentThread());
    
    auto testFilesDirectory = std::string("../../../TestFiles/");
    
    auto engine      = AudiumFactory::createAudiumEngine();
    auto zoomHandler = std::shared_ptr<ZoomHandler>(new ZoomHandler(engine->getAudioResourceContainer(),
                                                                    engine->getTransportSourceProvider()));
    
    // setting up a scrollbar is odd
    auto scrollbar = std::shared_ptr<juce::ScrollBar>(new juce::ScrollBar(false));
    scrollbar->setRangeLimits(0, 1200);
    scrollbar->setCurrentRange(0, 1200);
    zoomHandler->setHorizontalScrollBar(scrollbar.get());
    
    auto inFile = File(testFilesDirectory + "silence-fade.aiff");
    auto audioresource = engine->getAudioResourceContainer()->addAudioResource(juce::URL(inFile));
    
    engine->createDefaultRegionAndPlayList();
    
    auto waveFormComponent = std::shared_ptr<WaveFormComponent>(new WaveFormComponent(audioresource, engine->getPlayListContainer(), zoomHandler));
    waveFormComponent->setSize (1200, 400);
    // this is odd
    zoomHandler->setWidth(waveFormComponent->getWidth());
    
    // needed to find the colour
    auto lookAndFeel = std::shared_ptr<AudiumLookAndFeel>(new AudiumLookAndFeel());
    waveFormComponent->setLookAndFeel(lookAndFeel.get());
    
    while (!audioresource->isThumbnailFullyLoaded()) {
        Time::waitForMillisecondCounter(Time::getMillisecondCounter() + 10);
    }
    
    auto outFile = File(testFilesDirectory + "out.png");
    if (outFile.existsAsFile())
    {
        outFile.deleteFile();
    }
    
    writeComponentImageToFile(outFile, *waveFormComponent.get(), waveFormComponent->getBounds());

    
    
    
    waveFormComponent->setLookAndFeel(nullptr);
    
    
    zoomHandler = nullptr;
    waveFormComponent = nullptr;
    engine = nullptr;
    
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
    
    

}

