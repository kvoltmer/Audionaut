

#include "AudioRecorder.h"
#include "Engine/Resource/AudioResourceContainer.h"

namespace audium {

bool AudioRecorder::createThreadedWriter(const double sampleRate, const juce::File recordedFile)
{
    jassert(!recordedFile.existsAsFile());
    
    if (auto fileStream = std::unique_ptr<juce::FileOutputStream> (recordedFile.createOutputStream())) {
        juce::WavAudioFormat wavFormat;
        
        if (auto writer = wavFormat.createWriterFor (fileStream.get(), sampleRate, 1, 24, {}, 0))
        {
            // (passes responsibility for deleting the stream to the writer object that is now using it)
            fileStream.release();
            
            threadedWriter.reset (new juce::AudioFormatWriter::ThreadedWriter (writer, backgroundThread, 32768));
            
            return true;
        }
    }
    return false;
}

const juce::File AudioRecorder::prepareRecording(const int take,
                                                 const int channelNumber,
                                                 const double sampleRate)
{
    stop();
    
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

    const juce::ScopedLock sl (writerLock);
    activeWriter = threadedWriter.get();
    jassert(activeWriter);

}

const double AudioRecorder::getTotalLength() const
{
    if (thumbnail.load() != nullptr)
        return thumbnail.load()->getTotalLength();
    
    return 0.0;
}

} // namespace audium
