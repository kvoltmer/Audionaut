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

#include "Engine/AudiumEngine.h"
#include "Engine/Resource/AudioResourceContainer.h"

class ChannelsHeaderComponent  : public juce::Component
{
public:
    ChannelsHeaderComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine) :
        audiumEngine(audiumEngine)
    {
    }

    ~ChannelsHeaderComponent() override
    {
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff323232));
        
        
    }

    void resized() override
    {
    }
    

private:
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelsHeaderComponent)
};
