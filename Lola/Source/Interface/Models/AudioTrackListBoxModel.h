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
#include <vector>

#include <JuceHeader.h>
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Components/MiddlePanel/ArrangementView/AudioTrackComponent.h"
#include "Interface/Components/MiddlePanel/EditView/AudioTrackRegionEditComponent.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Controls/AudioTrackListBox.h"

class AudioTrackContainer;

class AudioTrackListBoxModel : public audium::ListBoxModel {
    
public:
    
    AudioTrackListBoxModel(std::shared_ptr<AudioTrackListBox> owner,
                           std::shared_ptr<audium::AudiumEngine> audiumEngine,
                           std::shared_ptr<ZoomHandler> zoomHandler,
                           std::shared_ptr<RegionSelector> regionSelector,
                           bool arrangementMode = true) :
        owner(owner),
        audiumEngine(audiumEngine),
        zoomHandler(zoomHandler),
        regionSelector(regionSelector),
        arrangementMode(arrangementMode)
    {
    }
    
    ~AudioTrackListBoxModel() override
    {
    }
    
    int getNumRows() override;

    void paintListBoxItem ( int rowNumber,
                            juce::Graphics& g,
                            int width, int height,
                            bool rowIsSelected) override;
    
    juce::Component* refreshComponentForRow (   int rowNumber, bool isRowSelected,
                                                juce::Component* existingComponentToUpdate) override;

    
    int getRowHeight (int rowNumber) const override;
    
    void deleteKeyPressed (int lastRowSelected) override;
    
    void backgroundClicked (const juce::MouseEvent&) override;
    
    void listWasScrolled() override;
    
    void selectedRowsChanged (int lastRowSelected) override;
    
    int getExtraSpaceAtBottom() const override;
        
private:
    std::shared_ptr<AudioTrackListBox> owner;
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<RegionSelector> regionSelector;
    
    bool arrangementMode = true;

};
