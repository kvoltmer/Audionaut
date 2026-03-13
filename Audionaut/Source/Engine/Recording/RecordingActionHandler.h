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
class PlayListScheduler;

class RecordingActionHandler {
    
public:
    
    RecordingActionHandler(std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
                           std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                           std::shared_ptr<TransportLoop> transportLoop_,
                           std::shared_ptr<PlayListScheduler> playListScheduler_);

    void onRecordingStarted();
    void onRecordingFinished();
    void onLoopEntered();
    void onLoopAction();
    void onPlayListItemUpdate();
    
private:
    std::shared_ptr<AudioTrackContainer> audioTrackContainer;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<TransportLoop> transportLoop;
    std::shared_ptr<PlayListScheduler> playListScheduler;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingActionHandler)

};

} // namespace audium
