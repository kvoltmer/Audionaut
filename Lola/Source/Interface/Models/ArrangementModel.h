//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrack.h"

class RegionSelector;
class ZoomHandler;

class ArrangementModel {
    
    
public:
    
    ArrangementModel(std::shared_ptr<audium::AudiumEngine>   audiumEngine,
                     std::shared_ptr<audium::AudioTrack>     audioTrack,
                     std::shared_ptr<RegionSelector> regionSelector,
                     std::shared_ptr<ZoomHandler>    zoomHandler);
    
    int getNumRows();

    juce::Component* refreshComponentForItem (int itemNumber, juce::Component* existingComponentToUpdate);
    
    const juce::Range<double> getRangeForItem(int itemNumber) const;

    void setAudioTrack(std::shared_ptr<audium::AudioTrack> track) { audioTrack = track; }
    
private:
    std::shared_ptr<audium::AudiumEngine>   audiumEngine;
    std::shared_ptr<audium::AudioTrack>     audioTrack;
    std::shared_ptr<RegionSelector> regionSelector;
    std::shared_ptr<ZoomHandler>    zoomHandler;
};
