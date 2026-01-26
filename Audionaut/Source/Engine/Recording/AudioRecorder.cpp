

#include "AudioRecorder.h"
#include "Engine/Resource/AudioResourceContainer.h"

namespace audium {

void AudioRecorder::startRecording(const int take,
                                   const int channelNumber,
                                   const double sampleRate)
{
    stop();
    
    if (sampleRate > 0) {
        auto file = AudioResourceContainer::getAudioRecordingFile(take, channelNumber);
        jassert(!file.existsAsFile());
        std::cout << "startRecording " << file.getFullPathName() << std::endl;
        
        if (auto fileStream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream())) {
            juce::WavAudioFormat wavFormat;
            
            if (auto writer = wavFormat.createWriterFor (fileStream.get(), sampleRate, 1, 24, {}, 0))
            {
                fileStream.release(); // (passes responsibility for deleting the stream to the writer object that is now using it)
                
                // Now we'll create one of these helper objects which will act as a FIFO buffer, and will
                // write the data to disk on our background thread.
                threadedWriter.reset (new juce::AudioFormatWriter::ThreadedWriter (writer, backgroundThread, 32768));
                
                // Reset our recording thumbnail
                if (thumbnail != nullptr)
                    thumbnail->reset (writer->getNumChannels(), writer->getSampleRate());
                nextSampleNum = 0;
                
                // And now, swap over our active writer pointer so that the audio callback will start using it..
                const juce::ScopedLock sl (writerLock);
                activeWriter = threadedWriter.get();
            }
        }
    }
}

} // namespace audium
