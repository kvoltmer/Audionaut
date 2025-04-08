//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/PlayList/LoopData.h"
#include "Engine/TimeContext.h"

namespace audium {

class TempoProvider;
class AudioTrackContainer;

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
