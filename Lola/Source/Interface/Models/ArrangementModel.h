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
