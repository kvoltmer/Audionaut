//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/Streamable.h"
#include "Engine/Group/AudioTrackContainer.h"

namespace audium
{

struct UndoableContainerAction final : public juce::UndoableAction
{
    UndoableContainerAction (AudioTrackContainer &container, bool rebuild = true) noexcept :
        container (container),
        rebuild (rebuild)
    {
        storeOldState();
    }
    
    ~UndoableContainerAction()
    {
        //std::cout << "~UndoableContainerAction" << std::endl;
    }
    
    void storeOldState()
    {
        juce::MemoryOutputStream outStream;
        container.writeToStream(outStream);
        oldMemoryBlock = outStream.getMemoryBlock();
    }
    
    void storeNewState()
    {
        juce::MemoryOutputStream outStream;
        container.writeToStream(outStream);
        newMemoryBlock = outStream.getMemoryBlock();
    }
    
    bool perform() override
    {
        try {
            juce::MemoryInputStream inputStream(newMemoryBlock, false);
            container.readFromStream(inputStream, rebuild);
        }
        catch (std::exception &e) {
            std::cout << "UndoableContainerAction::perform -> " << e.what() << std::endl;
            return false;
        }
        return true;
    }
    
    bool undo() override
    {
        try {
            juce::MemoryInputStream inputStream(oldMemoryBlock, false);
            container.readFromStream(inputStream, rebuild);
        }
        catch (std::exception &e) {
            std::cout << "UndoableContainerAction::undo -> " << e.what() << std::endl;
            return false;
        }
        return true;
    }
    
    int getSizeInUnits() override    { return container.getSizeInUnits(); }
    
    AudioTrackContainer &container;
    juce::MemoryBlock oldMemoryBlock;
    juce::MemoryBlock newMemoryBlock;
    bool rebuild = true;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoableContainerAction)
};

} // namespace audium
