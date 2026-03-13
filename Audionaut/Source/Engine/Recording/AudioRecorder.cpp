

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

    const juce::ScopedLock sl (writerLock);
    activeWriter = threadedWriter.get();
    jassert(activeWriter);

}

const double AudioRecorder::getTotalLength() const
{
    if (sampleRate > 0.0) {
        return samplesWritten / sampleRate;
    }
    
    return 0.0;
}

std::shared_ptr<audium::AudioThumbnail> AudioRecorder::getRecordingThumbnail() const
{
    return recordingThumbnail;
}


} // namespace audium
