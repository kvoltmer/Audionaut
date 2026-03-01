//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

namespace audium {

class AudioTrackContainer;
class AudioResourceContainer;
class TransportLoop;

class RecordingActionHandler {
    
public:
    
    RecordingActionHandler(std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
                           std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                           std::shared_ptr<TransportLoop> transportLoop_) :
        audioTrackContainer(audioTrackContainer_),
        audioResourceContainer(audioResourceContainer_),
        transportLoop(transportLoop_)
    {}
    
    void onRecordingFinished();
    void onLoopEntered();
    void onLoopAction();
    void onTimerUpdate();
    
private:
    std::shared_ptr<AudioTrackContainer> audioTrackContainer;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<TransportLoop> transportLoop;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingActionHandler)

};

} // namespace audium
