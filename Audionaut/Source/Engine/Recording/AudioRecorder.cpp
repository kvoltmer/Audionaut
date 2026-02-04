

#include "AudioRecorder.h"
#include "Engine/Resource/AudioResourceContainer.h"

namespace audium {

const juce::File AudioRecorder::startRecording(const int take,
                                               const int channelNumber,
                                               const double sampleRate)
{
    stop();
    
    if (sampleRate > 0) {
        auto recordedFile = AudioResourceContainer::getAudioRecordingFile(take, channelNumber);
        jassert(!recordedFile.existsAsFile());
        // std::cout << "startRecording " << recordedFile.getFileName() << std::endl;
        
        if (auto fileStream = std::unique_ptr<juce::FileOutputStream> (recordedFile.createOutputStream())) {
            juce::WavAudioFormat wavFormat;
            
            if (auto writer = wavFormat.createWriterFor (fileStream.get(), sampleRate, 1, 24, {}, 0))
            {
                // (passes responsibility for deleting the stream to the writer object that is now using it)
                fileStream.release();
                

                threadedWriter.reset (new juce::AudioFormatWriter::ThreadedWriter (writer, backgroundThread, 32768));
                
                nextSampleNum = 0;
                
                const juce::ScopedLock sl (writerLock);
                activeWriter = threadedWriter.get();
            }
        }
        return recordedFile;
    }
    return juce::File();
}

const double AudioRecorder::getTotalLength() const
{
    if (thumbnail != nullptr)
        return thumbnail->getTotalLength();
    
    return 0.0;
}

} // namespace audium
