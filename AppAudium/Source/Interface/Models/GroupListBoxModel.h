
#pragma once
#include <vector>

#include <JuceHeader.h>
#include "Engine/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Components/MiddlePanel/ArrangementView/ArrangementGroupComponent.h"
#include "Interface/Components/MiddlePanel/EditView/EditGroupComponent.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Controls/AudioGroupListBox.h"

class AudioGroupContainer;

class GroupListBoxModel : public audium::ListBoxModel {
    
public:
    
    GroupListBoxModel(std::shared_ptr<AudioGroupListBox> owner,
                           std::shared_ptr<AudiumEngine> audiumEngine,
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
    
    ~GroupListBoxModel() override
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
    

        
private:
    std::shared_ptr<AudioGroupListBox> owner;
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<RegionSelector> regionSelector;
    
    bool arrangementMode = true;

};
