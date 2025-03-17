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

#include "Engine/TimeContext.h"
#include "Engine/Region/AudioRegionData.h"

class AudioTrackContainer;
class AudioRegion;

class AudioRegionAdapter
{
    
public:
    
    AudioRegionAdapter(AudioTrackContainer &owner);
    ~AudioRegionAdapter() = default;
    
    const std::vector<std::shared_ptr<AudioRegion>> getAudioRegions() const;
    const std::vector<std::shared_ptr<AudioRegion>> getSelectedAudioRegions() const;
    std::shared_ptr<AudioRegion> getRegion(int rowNumber) const;
    
    void deselectAll();
    
    juce::SparseSet<int> getSelectedRows() const;
    void setSelectedRows(juce::SparseSet<int>& selectedRows);
    
    void createRegionsFromSelection(juce::String name, bool arrangementMode);
    void splitRegionsFromSelection(bool withUndo = true);
    
    void setSelectedRange(juce::Range<double> pos, audium::TimeContextType context);
    juce::Range<double> getSelectedRange(audium::TimeContextType context) const;
    bool anyRangeSelected() const;

private:
    AudioTrackContainer &owner;
    
    AudioRegionData::tRange selectedPositionClocks;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionAdapter)
};
