//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Widgets/audium_AudioThumbnail.h"

namespace audium {

class AudioRecorder : private juce::TimeSliceClient
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
    
    bool createThreadedWriter(const double sampleRate, const juce::File recordedFile);
    
    void createRecordingThumbnail(juce::AudioFormatManager &formatManager,
                                  juce::AudioThumbnailCache &thumbnailCache);
    
    const juce::File prepareRecording(const int take,
                                      const int channelNumber,
                                      const double sampleRate);
    
    void start();
    
    void stop();
    
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
            
//            std::cout << "rec " << buffer.getSample(0, 0) << std::endl;
            
            // write() returns false when the writer's fifo is full (the disk
            // thread fell behind); those samples never reach the file, so
            // they must not count towards the take length either.
            if (activeWriter.load()->write (buffer.getArrayOfReadPointers(), numSamples))
                samplesWritten += numSamples;
            else
                samplesDropped += numSamples;
            
            // The waveform thumbnail is fed from the recorder's background
            // thread (see drainThumbnailFifo): AudioThumbnail::addBlock
            // allocates and takes the lock the arrangement view holds while
            // painting, neither of which belongs in the audio callback.
            if (recordingThumbnail != nullptr)
                pushToThumbnailFifo (inputChannelData[0], numSamples);
        }
    }
        
    std::shared_ptr<audium::AudioThumbnail> getRecordingThumbnail() const;
        
    const double getTotalLength() const;
    
    /** Samples the disk writer refused because it fell behind (missing from the take). */
    juce::int64 getSamplesDropped() const noexcept { return samplesDropped.load(); }
    
private:
    
    // TimeSliceClient: drains the thumbnail fifo on backgroundThread
    int useTimeSlice() override;
    
    void pushToThumbnailFifo (const float* samples, int numSamples) noexcept;
    void drainThumbnailFifo();
    
    juce::TimeSliceThread backgroundThread { "Audio Recorder Thread" }; // the thread that will write our audio data to disk
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter; // the FIFO used to buffer the incoming data
    
    // audio thread -> background thread hand-off for the waveform thumbnail.
    // Sized in createRecordingThumbnail (message thread), before start().
    juce::AbstractFifo thumbnailFifo { 1 };
    juce::AudioBuffer<float> thumbnailRing;
    std::atomic<juce::int64> thumbnailSamplesSkipped { 0 }; // fifo overflow: leaves a gap instead of shifting the waveform
    juce::int64 nextSampleNum = 0; // background thread only
    
    std::atomic<juce::int64> samplesWritten { 0 };
    std::atomic<juce::int64> samplesDropped { 0 };
    
    double sampleRate = 0.0;
    
    juce::CriticalSection writerLock;
    
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter = nullptr;
        
    std::shared_ptr<audium::AudioThumbnail> recordingThumbnail = nullptr;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRecorder)

};

} // namespace audium
