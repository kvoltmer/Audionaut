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

#include "Engine/PlayList/LoopData.h"
#include "Engine/TimeContext.h"

class TempoProvider;
class AudioTrackContainer;

namespace audium {

class TransportLoop {
    
public:
    TransportLoop(std::shared_ptr<juce::UndoManager> undoManager_,
                  std::shared_ptr<TempoProvider> tempoProvider_) :
        undoManager(undoManager_),
        tempoProvider(tempoProvider_)
    {
    }
    ~TransportLoop() = default;
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);
    
    void setLoopPositionRange(std::shared_ptr<AudioTrackContainer> audioTrackContainer,
                              juce::Range<double> newRange,
                              audium::TimeContextType context);
    juce::Range<double> getLoopPositionRange(audium::TimeContextType context) const;
    
    bool isLoopActive() const;
    void setLoopActive(bool bActive);
    
    bool processLoop(double &thePosition, int numSamples);
    void reset();
    
    LoopData loopData;
    
private:
    
    std::shared_ptr<juce::UndoManager> undoManager;
    std::shared_ptr<TempoProvider> tempoProvider;
    
    double externalSampleRate = 44100.0;
    int loopCount = 0;
    bool withinLoop = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportLoop)
};

} // namespace audium
