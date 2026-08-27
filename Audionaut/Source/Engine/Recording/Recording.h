//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "AudioRecorder.h"
#include "Engine/Playback/PlaybackDefines.h"

namespace audium {

class Recording {

public:
    Recording();
    ~Recording();

    /**
     * @brief Global take counter: incremented by `AudioBusInterface::record`
     *        when a recording starts, read for naming the recorded file and
     *        its take region.
     */
    static int recordingCounter;

    void record(bool start, const int channelNumber, const double positionClocks);
    
    void setRecordEnabled(const int channelNumber,
                          bool bEnabled,
                          std::shared_ptr<AudioRecorder> recorder);
            
    std::shared_ptr<AudioRecorder> getAudioRecorder(int channelNumber);
    
    const juce::File getRecordedFile(const int channelNumber) const
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS)
            return recordedFiles[channelNumber];
        
        return juce::File();
    }

    void setRecordedFile(const int channelNumber, const juce::File file)
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS)
            recordedFiles[channelNumber] = file;
    }
    
    void setSampleRate(double sampleRate_) { sampleRate = sampleRate_; }
        
    const double getRecordingStartPosition(int c) const { return recordingStartPositionClocks[c]; }
    
private:
    
    std::map<int, std::shared_ptr<AudioRecorder>> recorders;
    
    juce::File recordedFiles[MAX_AUDIO_CHANNELS];
    
    double recordingStartPositionClocks[MAX_AUDIO_CHANNELS];
    
    double sampleRate = 0.0;
    
    
    juce::AudioFormatManager formatManager;

    juce::AudioThumbnailCache audioThumbnailCache;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Recording)
};

} // namespace audium
