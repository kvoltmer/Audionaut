//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Interface/Controls/DraggerControl.h"

#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/Export/PlayListItemExport.h"
#include "Engine/Analysis/AnalysisProvider.h"

class PlayListItemDraggerControl : public DraggerControl,
                                   public juce::ChangeListener
{
public:
    
    PlayListItemDraggerControl(std::shared_ptr<audium::AudiumEngine> audiumEngine_,
                               std::shared_ptr<audium::PlayListContainer> playListContainer_,
                               std::shared_ptr<ZoomHandler> zoomHandler_,
                               juce::Colour colour_,
                               std::shared_ptr<RegionSelector> regionSelector_) :
        DraggerControl(audiumEngine_,
                       zoomHandler_,
                       colour_,
                       regionSelector_),
        playListContainer(playListContainer_)
    {
        regionSelector->playListItemDraggerControls.push_back(this);

        // The BPM suffix is computed live at paint-time from the analysis
        // cache (see getBpmSuffix()), so a repaint is all that's needed to
        // pick up a background analysis that just finished.
        if (auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider())
            analysisProvider->addChangeListener(this);
    }

    ~PlayListItemDraggerControl() override
    {
        if (auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider())
            analysisProvider->removeChangeListener(this);

        std::erase_if(regionSelector->playListItemDraggerControls, [this](const auto* item) {
            return item == this;
        });
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        repaint();
    }


    bool isSelected() const override
    {
        return playListItem->isSelected();
    }
    
    void setSelected(bool bSelected, bool deselectOthers) override
    {
        if (deselectOthers)
        {
            audiumEngine->getAudioTrackContainer()->getSelectionManager()->deselectAll();
        }
        playListItem->setSelected(bSelected);
        audiumEngine->getAudioTrackContainer()->sendActionMessage(audium::updateSelection);
    }
    
    void shiftSelect() override;

    const juce::String getLabelString() const override
    {
        return playListItem->getRegion()->getName();
    }

    const juce::String getLabelSuffix() const override;

    const juce::Colour getLabelColour() const override
    {
        return playListItem->getRegion()->getAudioTrack()->getViewState().getColour();
    }
    
    bool validateData() override;
    
    void setPlayListItem(std::shared_ptr<audium::PlayListItem> playListItem_) { playListItem = playListItem_; }
    
    void mouseDown (const juce::MouseEvent& e) override;
    
    void exportSelectedPlayListItem();
    
    bool isRecording() override
    {
        return playListItem->isRecording();
    }

private:
    // BPM estimate(s) formatted for the label, e.g. "128.0 BPM" for a single
    // file or "(128.0, 132.0) BPM" when the region spans several files with
    // differing estimates. Empty if no BPM estimate is available.
    juce::String getBpmSuffix() const;

    std::shared_ptr<audium::PlayListContainer> playListContainer;
    std::shared_ptr<audium::PlayListItem> playListItem;
    std::unique_ptr<audium::PlayListItemExport> exporter;
    
};
