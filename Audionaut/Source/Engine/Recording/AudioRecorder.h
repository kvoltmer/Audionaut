//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Widgets/audium_AudioThumbnail.h"

namespace audium {

class AudioRecorder
{
public:
    AudioRecorder ()
    {
        backgroundThread.startThread();
    }
    
    ~AudioRecorder()
    {
        stop();
    }
    
    const juce::File startRecording(const int take,
                                    const int channelNumber,
                                    const double sampleRate);
    
    void stop()
    {
        // First, clear this pointer to stop the audio callback from using our writer object..
        {
            const juce::ScopedLock sl (writerLock);
            activeWriter = nullptr;
            thumbnail = nullptr;
        }
        
        // Now we can delete the writer object. It's done in this order because the deletion could
        // take a little time while remaining data gets flushed to disk, so it's best to avoid blocking
        // the audio callback while this happens.
        threadedWriter.reset();
    }
    
    bool isRecording() const
    {
        return activeWriter.load() != nullptr;
    }
    
    template <typename ProcessContext>
    void process (const ProcessContext& context) noexcept
    {
        const juce::ScopedLock sl (writerLock);
        
        if (activeWriter.load() != nullptr) {
            
            auto& inputBlock = context.getInputBlock();
            auto inputChannels = static_cast<int>(inputBlock.getNumChannels());
            auto numSamples = static_cast<int>(inputBlock.getNumSamples());
            jassert(inputChannels == 1);
            
            const float* inputChannelData[1];
            inputChannelData[0] = inputBlock.getChannelPointer (0);
            
            juce::AudioBuffer<float> buffer (const_cast<float**> (inputChannelData),
                                             inputChannels,
                                             numSamples);
            
            activeWriter.load()->write (buffer.getArrayOfReadPointers(), numSamples);
            
            if (thumbnail != nullptr) {
                thumbnail->addBlock (nextSampleNum, buffer, 0, numSamples);
                nextSampleNum += numSamples;
            }
        }
    }
    
    void setAudioThumbnail(AudioThumbnail *thumbnail_) { thumbnail = thumbnail_; }
        
    const double getTotalLength() const;
    
private:
    
    AudioThumbnail *thumbnail = nullptr;
    
    juce::TimeSliceThread backgroundThread { "Audio Recorder Thread" }; // the thread that will write our audio data to disk
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter; // the FIFO used to buffer the incoming data
    
    juce::int64 nextSampleNum = 0;
    
    juce::CriticalSection writerLock;
    
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    
};

} // namespace audium
