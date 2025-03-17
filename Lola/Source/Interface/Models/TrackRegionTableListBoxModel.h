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
#include "Engine/Region/AudioRegion.h"
#include "Engine/Region/AudioRegionContainer.h"


class TrackRegionTableListBoxModel  : public juce::TableListBoxModel
{
public:
    TrackRegionTableListBoxModel(std::shared_ptr<juce::TableListBox> owner,
                                 std::shared_ptr<audium::AudiumEngine> audiumEngine);
    ~TrackRegionTableListBoxModel() override;

    int getNumRows() override;
    
    void paintRowBackground (juce::Graphics&,
                                     int rowNumber,
                                     int width, int height,
                                     bool rowIsSelected) override
    {
    }

    void paintCell (juce::Graphics&,
                            int rowNumber,
                            int columnId,
                            int width, int height,
                            bool rowIsSelected) override
    {
    }
    
    juce::Component* refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected,
                                                juce::Component* existingComponentToUpdate) override;
    
    void selectedRowsChanged (int lastRowSelected) override;
    
    void deleteKeyPressed (int lastRowSelected) override;
    
    void backgroundClicked (const juce::MouseEvent&) override;
    
    void cellClicked (int rowNumber, int columnId, const juce::MouseEvent&) override;
    
    juce::var getDragSourceDescription (const juce::SparseSet<int>& currentlySelectedRows) override;

private:
    
    std::shared_ptr<audium::AudioRegion> getRegion(int rowNumber, int columnId) const;
    
    std::shared_ptr<juce::TableListBox> owner;
    
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackRegionTableListBoxModel)
};
