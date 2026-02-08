//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut xuses a GPL/commercial licence - see LICENCE.md for details.

#include "Recording.h"
#include "Engine/AudiumEngine.h"

namespace audium {

void Recording::record(bool start, const int channelNumber)
{
    auto take = AudiumEngine::recordingCounter;
    if (start)
        AudiumEngine::recordingCounter++;
    
    if (channelNumber < 0) {
        
        for (auto i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
            if (auto recorder = getAudioRecorder(i)) {
                if (start) {
                    auto file = recorder->prepareRecording(take, i, getSampleRate());
                    setRecordedFile(i, file);
                    recorder->start();
                }
                else {
                    recorder->stop();
                }
            }
        }
    }
    else {
        if (auto recorder = getAudioRecorder(channelNumber)) {
            if (start) {
                auto file = recorder->prepareRecording(take,
                                                       channelNumber,
                                                       getSampleRate());
                setRecordedFile(channelNumber, file);
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
    std::cout << "setRecordEnabled " << channelNumber << " " << bEnabled << std::endl;

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

void Recording::setRecordingThumbnail(AudioThumbnail *audioThumbnail,
                                                         int channelNumber)
{
    if (recorders.find(channelNumber) != recorders.end()) {
        recorders[channelNumber]->setAudioThumbnail(audioThumbnail);
    }
    else {
        jassertfalse;
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
