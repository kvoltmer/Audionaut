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

namespace audium
{

struct UndoableContainerAction final : public juce::UndoableAction
{
    UndoableContainerAction (std::shared_ptr<Streamable> container) noexcept
        : container (container)
    {
    }
    
    ~UndoableContainerAction()
    {
        container = nullptr;
        std::cout << "~UndoableContainerAction" << std::endl;
    }
    
    void storeOldState()
    {
        juce::MemoryOutputStream outStream;
        container->writeToStream(outStream);
        oldMemoryBlock = outStream.getMemoryBlock();
    }
    
    void storeNewState()
    {
        juce::MemoryOutputStream outStream;
        container->writeToStream(outStream);
        newMemoryBlock = outStream.getMemoryBlock();
    }
    
    bool perform() override
    {
        juce::MemoryInputStream inputStream(newMemoryBlock, false);
        container->readFromStream(inputStream);
        return true;
    }
    
    bool undo() override
    {
        juce::MemoryInputStream inputStream(oldMemoryBlock, false);
        container->readFromStream(inputStream);
        return true;
    }
    
    int getSizeInUnits() override    { return container->getSizeInUnits(); }
    
    std::shared_ptr<Streamable> container;
    juce::MemoryBlock oldMemoryBlock;
    juce::MemoryBlock newMemoryBlock;
    
    JUCE_DECLARE_NON_COPYABLE (UndoableContainerAction)
};

} // namespace audium
