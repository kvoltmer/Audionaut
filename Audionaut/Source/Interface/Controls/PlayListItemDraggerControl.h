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
                                   public juce::ChangeListener,
                                   public juce::ActionListener
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

        // The BPM suffix and the grid-match check mark are derived from the
        // analysis cache, memoised against their inputs (see
        // updateAnalysisPaintCache()), so a repaint is all that's needed to
        // pick up a background analysis that just finished.
        if (auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider())
            analysisProvider->addChangeListener(this);

        // The grid-match check mark also depends on the project tempo.
        if (auto tempoProvider = playListContainer->getTempoProvider())
            tempoProvider->addActionListener(this);
    }

    ~PlayListItemDraggerControl() override
    {
        if (auto tempoProvider = playListContainer->getTempoProvider())
            tempoProvider->removeActionListener(this);

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

    void actionListenerCallback (const juce::String& message) override
    {
        if (message == audium::tempoChanged)
            repaint();
    }

    void paint (juce::Graphics& g) override;


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

    // Whether every analysed resource of the item sits on the project's beat
    // grid (see AnalysisProvider::matchesGrid). False when nothing has been
    // analysed yet.
    bool matchesProjectGrid() const;

    // Both of the above are served from this memo - the analysis lookups
    // behind them (locked cache queries, a beat-vector copy per resource)
    // are too expensive to run on every paint, and the playhead stripe
    // repaints this control at timer rate during playback.
    struct AnalysisPaintCache {
        bool valid = false;
        std::uint64_t generation = 0;
        const void* item = nullptr;
        double tempo = 0.0;
        double startClocks = 0.0;
        juce::Range<double> regionSeconds;
        juce::String bpmSuffix;
        bool gridMatch = false;
    };

    void updateAnalysisPaintCache() const;

    mutable AnalysisPaintCache analysisPaintCache;

    // Small green check mark at the right end of the dragger strip, shown
    // while matchesProjectGrid() holds.
    void paintGridMatchCheckMark (juce::Graphics& g) const;

    std::shared_ptr<audium::PlayListContainer> playListContainer;
    std::shared_ptr<audium::PlayListItem> playListItem;
    std::unique_ptr<audium::PlayListItemExport> exporter;
    
};
