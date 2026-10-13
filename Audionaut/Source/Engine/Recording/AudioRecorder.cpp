

#include "AudioRecorder.h"
#include "Engine/Resource/AudioResourceContainer.h"

namespace audium {

bool AudioRecorder::createThreadedWriter(const double sampleRate_, const juce::File recordedFile)
{
    jassert(!recordedFile.existsAsFile());
    
    std::unique_ptr<OutputStream> outStream (recordedFile.createOutputStream());
    if (outStream != nullptr) {
        WavAudioFormat wav;
        auto opt = AudioFormatWriter::Options{}.withSampleRate (sampleRate)
                                                .withNumChannels (1)
                                                .withBitsPerSample (24);
        auto writer = wav.createWriterFor (outStream, opt);
        if (writer != nullptr) {
            threadedWriter.reset (new AudioFormatWriter::ThreadedWriter (writer.release(),                                                                                     backgroundThread,
                                                                         32768));
            return true;
        }
    }
    return false;
}

void AudioRecorder::createRecordingThumbnail(juce::AudioFormatManager &formatManager,
                                             juce::AudioThumbnailCache &thumbnailCache)
{
    // create thumbnail
    auto sourceSamplesPerThumbnailSample = 64;
    recordingThumbnail = std::make_shared<audium::AudioThumbnail>(sourceSamplesPerThumbnailSample,
                                                                  formatManager,
                                                                  thumbnailCache);
    jassert(sampleRate > 0.0);
    recordingThumbnail->reset(1, sampleRate);
    
    // Ring for the audio-thread -> background-thread hand-off: a few seconds
    // so a stalled message thread (modal dialog, heavy paint) only delays the
    // waveform instead of dropping it. Allocated here, never in process().
    const auto capacity = juce::jmax (1 << 16, (int) (sampleRate * 4.0));
    if (thumbnailRing.getNumSamples() != capacity)
        thumbnailRing.setSize (1, capacity);
    thumbnailFifo.setTotalSize (capacity);
}

const juce::File AudioRecorder::prepareRecording(const int take,
                                                 const int channelNumber,
                                                 const double sampleRate_)
{
    stop();
    sampleRate = sampleRate_;
    
    if (sampleRate > 0) {
        auto recordedFile = AudioResourceContainer::getAudioRecordingFile(take, channelNumber);
        if (createThreadedWriter(sampleRate, recordedFile)) {
            return recordedFile;
        }
    }
    return juce::File();
}

void AudioRecorder::start()
{
    nextSampleNum = 0;
    samplesWritten = 0;
    samplesDropped = 0;
    thumbnailSamplesSkipped = 0;
    thumbnailFifo.reset();
    
    if (recordingThumbnail != nullptr)
        backgroundThread.addTimeSliceClient (this);

    const juce::ScopedLock sl (writerLock);
    activeWriter = threadedWriter.get();
    jassert(activeWriter);
}

void AudioRecorder::stop()
{
    // First, clear this pointer to stop the audio callback from using our writer object..
    {
        const juce::ScopedLock sl (writerLock);
        activeWriter = nullptr;
    }
    
    // Nothing is pushed any more: take the drain off the background thread
    // (this waits for a running useTimeSlice to return) and flush the rest so
    // the thumbnail is complete when the take is finished.
    backgroundThread.removeTimeSliceClient (this);
    drainThumbnailFifo();
    
    // Now we can delete the writer object. It's done in this order because the deletion could
    // take a little time while remaining data gets flushed to disk, so it's best to avoid blocking
    // the audio callback while this happens.
    threadedWriter.reset();
}

int AudioRecorder::useTimeSlice()
{
    drainThumbnailFifo();
    return 20; // ms - the arrangement view repaints at most this often for a live take
}

void AudioRecorder::pushToThumbnailFifo (const float* samples, int numSamples) noexcept
{
    if (thumbnailFifo.getFreeSpace() < numSamples) {
        // never block, never grow: leave a gap in the waveform instead
        thumbnailSamplesSkipped += numSamples;
        return;
    }
    
    const auto scope = thumbnailFifo.write (numSamples);
    if (scope.blockSize1 > 0)
        thumbnailRing.copyFrom (0, scope.startIndex1, samples, scope.blockSize1);
    if (scope.blockSize2 > 0)
        thumbnailRing.copyFrom (0, scope.startIndex2, samples + scope.blockSize1, scope.blockSize2);
}

void AudioRecorder::drainThumbnailFifo()
{
    if (recordingThumbnail == nullptr)
        return;
    
    const auto ready = thumbnailFifo.getNumReady();
    if (ready > 0) {
        const auto scope = thumbnailFifo.read (ready);
        if (scope.blockSize1 > 0) {
            recordingThumbnail->addBlock (nextSampleNum, thumbnailRing, scope.startIndex1, scope.blockSize1);
            nextSampleNum += scope.blockSize1;
        }
        if (scope.blockSize2 > 0) {
            recordingThumbnail->addBlock (nextSampleNum, thumbnailRing, scope.startIndex2, scope.blockSize2);
            nextSampleNum += scope.blockSize2;
        }
    }
    
    nextSampleNum += thumbnailSamplesSkipped.exchange (0);
}

const double AudioRecorder::getTotalLength() const
{
    if (sampleRate > 0.0) {
        return static_cast<double> (samplesWritten.load()) / sampleRate;
    }
    
    return 0.0;
}

std::shared_ptr<audium::AudioThumbnail> AudioRecorder::getRecordingThumbnail() const
{
    return recordingThumbnail;
}


} // namespace audium
