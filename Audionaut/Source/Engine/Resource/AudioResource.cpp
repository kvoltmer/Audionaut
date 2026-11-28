//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Engine/Resource/AudioResource.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/AudioSources/VoiceSourceContainer.h"
#include "Engine/AudioSources/VoiceSource.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/Region/AudioRegionContainer.h"


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

std::shared_ptr<VoiceSource> AudioResource::createNewTransportSource()
{
    auto voiceSource = audioTrack->getVoiceSourceContainer()->createAndAddVoiceSource(*this);
    
    auto sampleRate = 44100.0;
    auto numSamples = 512;
    auto device = owner.getAudioDeviceManager()->getCurrentAudioDevice();
    if (device != nullptr) {
        sampleRate = device->getCurrentSampleRate();
        numSamples = device->getCurrentBufferSizeSamples();
    }
    
    voiceSource->prepareToPlay(numSamples, sampleRate);
    
    return voiceSource;
}

const juce::String AudioResource::getFileNameWithoutExtension() const
{
    if (url.isLocalFile())
        return url.getLocalFile().getFileNameWithoutExtension();
    
    return "";
}

const juce::String AudioResource::getFullPathName() const
{
    if (url.isLocalFile())
        return url.getLocalFile().getFullPathName();
    
    return "not a local file";
}

juce::File AudioResource::getLocalFile() const
{
    if (url.isLocalFile())
        return url.getLocalFile();

    return {};
}

const juce::String AudioResource::getRelativePath(const juce::File &directoryToBeRelativeTo) const
{
    if (url.isEmpty() && isRecording()) {
        auto outChannel = getOutputChannelNumber();
        auto file = getAudioTrack()->getAudioTrackContainer().audioBusInterface->getRecordedAudioFile(outChannel);
        return file.getRelativePathFrom(directoryToBeRelativeTo);
    }
    else if (url.isLocalFile())
        return url.getLocalFile().getRelativePathFrom(directoryToBeRelativeTo);
    else
        return url.toString(false);
}

double AudioResource::getSampleRate() const
{
    if (audioFormatReader != nullptr) {
        return audioFormatReader->sampleRate;
    }
    else if (auto device = getContainer().getAudioDeviceManager()->getCurrentAudioDevice()) {
        return device->getCurrentSampleRate();
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

unsigned int AudioResource::getBitDepth() const
{
    if (audioFormatReader != nullptr) {
        return audioFormatReader->bitsPerSample;
    }
    return 24;
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

double AudioResource::getRecordedLength(audium::TimeContextType context) const
{
    auto outChannel = getOutputChannelNumber();
    auto audioBusInterface = getAudioTrack()->getAudioTrackContainer().audioBusInterface;
    auto length = audioBusInterface->getRecordedLength(outChannel);
    
    if (context == audium::seconds) {
        return length;
    }
    else if (context == audium::clocks) {
        return owner.getTempoProvider()->secondsToClocks(length);
    }

    jassertfalse;
    return length;
}

int AudioResource::getOutputChannelNumber() const
{
    auto channelOffset = getAudioTrack()->getChannelOffset();
    auto dstChannel = getChannelMapping().getDestinationChannel();
    return dstChannel + channelOffset;
}

bool AudioResource::loadRecordedAudioFile()
{
    auto audioBusChannel = getOutputChannelNumber();
    auto audioBusInterface = getAudioTrack()->getAudioTrackContainer().audioBusInterface;
    
    // audio resrouce without audioFormatReader -> recording
    if (audioFormatReader == nullptr &&
        not audioBusInterface->isRecording(audioBusChannel)) {
        auto file = audioBusInterface->getRecordedAudioFile(audioBusChannel);
        
        if (file.existsAsFile()) {
            juce::URL theUrl(file);
            setUrl(theUrl);
            audioFormatReader = getAudioTrack()->getAudioResourceContainer().getAudioFormatReaderForUrl(theUrl);
            jassert(audioFormatReader);
            return (audioFormatReader != nullptr);
        }
        else {
            // hu?
            jassertfalse;
        }
    }
    return false;
}

bool AudioResource::writeToJson (json& output)
{
    output["relative_file_path"]    = getRelativePath(owner.getSerializationBaseDirectory()).toStdString();
    output["number_of_channels"]    = getNumAudioFileChannels();
    if (not isRecording())
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

const juce::URL AudioResource::urlFromJson (json& input, const juce::File& baseDirectory)
{
    juce::String filePath;
    
    // relative path is always a local file
    if (input.contains("relative_file_path")) {
        juce::String relPath = input["relative_file_path"].template get<std::string>();
        filePath = baseDirectory.getChildFile(relPath).getFullPathName();
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
    else {
        lengthInSeconds = getFileLength(audium::seconds);
    }
    return channelMapping->readFromJson(input, rebuild);
}

} // namespace audium
