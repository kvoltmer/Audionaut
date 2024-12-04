/*
  ==============================================================================

    UndoableContainerAction.h
    Created: 1 Feb 2024 1:46:34pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

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
        juce::MemoryInputStream inputStream(newMemoryBlock, false);
        container.readFromStream(inputStream, rebuild);
        return true;
    }
    
    bool undo() override
    {
        juce::MemoryInputStream inputStream(oldMemoryBlock, false);
        container.readFromStream(inputStream, rebuild);
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
