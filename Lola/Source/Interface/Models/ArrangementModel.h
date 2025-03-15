/*
  ==============================================================================

    ArrangementModel.h
    Created: 14 Mar 2025 10:50:54am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

class PlayListContainer;
class AudiumEngine;
class AudioTrack;
class RegionSelector;
class ZoomHandler;

class ArrangementModel {
    
    
public:
    
    ArrangementModel(std::shared_ptr<AudiumEngine>   audiumEngine,
                     std::shared_ptr<AudioTrack>     audioTrack,
                     std::shared_ptr<RegionSelector> regionSelector,
                     std::shared_ptr<ZoomHandler>    zoomHandler);
    
    int getNumRows();

    juce::Component* refreshComponentForItem (int itemNumber, juce::Component* existingComponentToUpdate);
    
    const juce::Range<double> getRangeForItem(int itemNumber) const;

    void setAudioTrack(std::shared_ptr<AudioTrack> track) { audioTrack = track; }
    
private:
    std::shared_ptr<AudiumEngine>   audiumEngine;
    std::shared_ptr<AudioTrack>     audioTrack;
    std::shared_ptr<RegionSelector> regionSelector;
    std::shared_ptr<ZoomHandler>    zoomHandler;
};
