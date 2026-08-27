//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Recording.h"


namespace audium {

int Recording::recordingCounter = 0;

Recording::Recording() :
    audioThumbnailCache(64)
{
    formatManager.registerBasicFormats();
}

Recording::~Recording()
{
    recorders.clear();
}

void Recording::record(bool start,
                       const int channelNumber,
                       const double positionClocks)
{
    if (channelNumber < 0) {
        
        for (auto i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
            if (auto recorder = getAudioRecorder(i)) {
                record(start, i, positionClocks); // recursion
            }
        }
    }
    else {
        if (auto recorder = getAudioRecorder(channelNumber)) {
            if (start) {
                recordingStartPositionClocks[channelNumber] = positionClocks;
                // std::cout << "start rec " << channelNumber << " pos " << positionClocks << std::endl;
                setRecordedFile(channelNumber, recorder->prepareRecording(Recording::recordingCounter,
                                                                          channelNumber,
                                                                          sampleRate));
                recorder->createRecordingThumbnail(formatManager, audioThumbnailCache);
                recorder->start();
            }
            else {
                recorder->stop();
            }
        }
        else {
            // hu?
            jassertfalse;
        }
    }
}

void Recording::setRecordEnabled(const int channelNumber,
                                 bool bEnabled,
                                 std::shared_ptr<AudioRecorder> recorder)
{
    //std::cout << "setRecordEnabled " << channelNumber << " " << bEnabled << std::endl;

    if (bEnabled) {
        if (recorders.find(channelNumber) == recorders.end()) {
            recorders.insert(std::make_pair(channelNumber, recorder));
        }
    }
    else {
        // erase recorder if exists at channel number
        if (recorders.find(channelNumber) != recorders.end()) {
            recorders.erase(channelNumber);
        }
    }
}

std::shared_ptr<AudioRecorder> Recording::getAudioRecorder(int channelNumber)
{
    if (recorders.find(channelNumber) != recorders.end()) {
        return recorders[channelNumber];
    }
    return nullptr;
}

} // namespace audium
