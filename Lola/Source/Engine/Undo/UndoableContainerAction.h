//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
