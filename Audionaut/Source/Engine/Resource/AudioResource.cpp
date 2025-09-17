//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Engine/Resource/AudioResource.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/Factory/AudioResourceFactory.h"
#include "Engine/Resource/ChannelMapping.h"

namespace audium {

AudioResource::AudioResource(AudioResourceContainer& audioResourceContainer_,
                             std::shared_ptr<AudioTrack> audioTrack_,
                             std::shared_ptr<ResourceGroup> resourceGroup_,
                             juce::URL url_,
                             std::shared_ptr<juce::AudioFormatReader> reader_,
                             int destChannel,
                             int sourceChannel) :
    audioFormatReader(reader_),
    owner(audioResourceContainer_),
    audioTrack(audioTrack_),
    resourceGroup(resourceGroup_),
    url(url_)
{
    jassert(audioFormatReader != nullptr);
    
    channelMapping = std::make_unique<audium::ChannelMapping>();
    
    if (destChannel >= 0 &&
        sourceChannel >= 0) {
        audioTrack->ensureNumChannels(destChannel + 1);
        channelMapping->setOutputChannelMapping(sourceChannel, destChannel);
    }
}


AudioResource::~AudioResource()
{
}

std::shared_ptr<AudiumTransportSource> AudioResource::createNewTransportSource(std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource)
{
    auto transportSource = audioTrack->getTransportSourceContainer()->createAndAddTransportSource(*this, audioFormatReaderSource);
    
    auto sampleRate = 44100.0;
    auto numSamples = 512;
    auto device = owner.getAudioDeviceManager()->getCurrentAudioDevice();
    if (device != nullptr) {
        sampleRate = device->getCurrentSampleRate();
        numSamples = device->getCurrentBufferSizeSamples();
    }
    
    transportSource->prepareToPlay(numSamples, sampleRate);
    
    return transportSource;
}

const juce::String AudioResource::getFileNameWithoutExtension() const
{
    if (url.isLocalFile())
        return url.getLocalFile().getFileNameWithoutExtension();
    
    return "n/a";
}

const juce::String AudioResource::getFullPathName() const
{
    if (url.isLocalFile())
        return url.getLocalFile().getFullPathName();
    
    return "not a local file";
}

const juce::String AudioResource::getUrlAsString() const
{
    return url.toString(true);
}

const juce::String AudioResource::getRelativePath(const juce::File &directoryToBeRelativeTo) const
{
    return url.getLocalFile().getRelativePathFrom(directoryToBeRelativeTo);
}

double AudioResource::getSampleRate() const
{
    if (audioFormatReader != nullptr) {
        return audioFormatReader->sampleRate;
    }
    return 44100.0;
}

unsigned int AudioResource::getNumAudioFileChannels() const
{
    if (audioFormatReader != nullptr) {
        return audioFormatReader->numChannels;
    }
    return numChannels;
}

double AudioResource::getFileLength(audium::TimeContextType context) const
{
    auto length = lengthInSeconds;
    
    if (audioFormatReader != nullptr &&
        audioFormatReader->sampleRate > 0.0) {
        length = audioFormatReader->lengthInSamples / audioFormatReader->sampleRate;
    }
    
    if (context == audium::seconds) {
        return length;
    }
    else if (context == audium::clocks) {
        return owner.getTempoProvider()->secondsToClocks(length);
    }
    
    jassertfalse;
    return 0.0;
}

std::vector<std::shared_ptr<AudioResource>> AudioResource::getAudioResourcesWithinResourceGroup() const
{
    std::vector<std::shared_ptr<AudioResource>> result;
    auto resources = owner.getAudioResourcesForResourceGroup(resourceGroup.get());
    
    for (auto resource : resources) {
        if (resource.get() == this)
            continue;
        
        result.push_back(resource);
    }
    return result;
}

bool AudioResource::containsAbsolutePosition(double position, audium::TimeContextType context) const
{
    auto startTime = getResourceGroup()->getAbsolutePosition(context);
    auto endTime = startTime + getResourceGroup()->getRegionData(context).getLength();
    juce::Range<double> absoluteRange(startTime, endTime);
    if (absoluteRange.contains(position)) {
        return true;
    }
    
    return false;
}

bool AudioResource::writeToJson (json& output)
{
    output["relative_file_path"]    = getRelativePath(AudiumEngine::projectDirectory).toStdString();
    output["number_of_channels"]    = getNumAudioFileChannels();
    output["length_in_seconds"]     = getFileLength(audium::seconds);
    
    channelMapping->writeToJson(output);
    
    return true;
}

void AudioResource::testUrl (const juce::URL& url)
{
    if (url.isLocalFile()) {
        juce::File file = url.getLocalFile();
        auto fin = std::make_unique<juce::FileInputStream> (file);
        
        if (!fin->openedOk()) {
            throw std::runtime_error(file.getFullPathName().toStdString() + "\nError: " + fin->getStatus().getErrorMessage().toStdString());
        }
    }
}

const juce::URL AudioResource::urlFromJson (json& input)
{
    juce::String filePath;
    
    // relative path is always a local file
    if (input.contains("relative_file_path")) {
        juce::String relPath = input["relative_file_path"].template get<std::string>();
        filePath = AudiumEngine::projectDirectory.getChildFile(relPath).getFullPathName();
        if (File(filePath).existsAsFile())
            return URL(File(filePath));
    }
    
    std::cout << "warning: relative path does not exist: " << filePath << std::endl;
    
    if (input.contains("absolute_file_path")) {
        filePath = input["absolute_file_path"].template get<std::string>();
        
        juce::URL absolute_url(filePath);
        if (absolute_url.getLocalFile().existsAsFile()) {
            return absolute_url;
        }
    }
    
    std::cout << "error: absolute path does not exist: " << filePath << std::endl;
    throw std::runtime_error(filePath.toStdString() + "\n\nError: File not found.");
    
    return URL(File(filePath));
}

bool AudioResource::readFromJson (json& input, bool rebuild)
{
    if (input.contains("number_of_channels"))
        numChannels = input["number_of_channels"].template get<int>();
    
    if (input.contains("length_in_seconds"))
        lengthInSeconds = input["length_in_seconds"].template get<double>();
    
    
    if (! channelMapping->readFromJson(input, rebuild)) {
        return false;
    }
    return true;
}

} // namespace audium
