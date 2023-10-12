
#pragma once
#include <vector>

#include <JuceHeader.h>
#include "Engine/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Components/AudioGroupComponent.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Controls/AudioGroupListBox.h"
#include "Interface/Controls/RegionSelector.h"

class AudioGroupContainer;

class AudioGroupListBoxModel : public audium::ListBoxModel {
    
public:
    
    AudioGroupListBoxModel(std::shared_ptr<AudioGroupListBox> owner,
                           std::shared_ptr<AudiumEngine> audiumEngine,
                           std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                           std::shared_ptr<AudioGroupContainer> audioGroupContainer,
                           std::shared_ptr<ZoomHandler> zoomHandler,
                           std::shared_ptr<RegionSelector> RegionSelector);
    ~AudioGroupListBoxModel();
    
    int getNumRows() override;

    void paintListBoxItem ( int rowNumber,
                            juce::Graphics& g,
                            int width, int height,
                            bool rowIsSelected) override;
    
    juce::Component* refreshComponentForRow (   int rowNumber, bool isRowSelected,
                                                juce::Component* existingComponentToUpdate) override;

    
    int getRowHeight (int rowNumber) const override;
    
    void selectedRowsChanged (int lastRowSelected) override;
    
    void deleteKeyPressed (int lastRowSelected) override;
    
    void listWasScrolled() override;
        
private:
    std::shared_ptr<AudioGroupListBox> owner;
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<AudioGroupContainer> audioGroupContainer;
    std::shared_ptr<ZoomHandler> zoomHandler;
    
    /// remove this
    std::shared_ptr<RegionSelector> regionSelector;
    
    

};
