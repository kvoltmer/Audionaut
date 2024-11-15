/*
  ==============================================================================

    AudioResource.cpp
    Created: 29 Jan 2023 12:55:52pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Engine/Resource/AudioResource.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/Factory/AudioResourceFactory.h"
#include "Engine/Resource/ChannelMapping.h"

AudioResource::AudioResource(AudioResourceContainer& audioResourceContainer,
                             std::shared_ptr<AudioTrack> audioTrack,
                             std::shared_ptr<AudioSubGroup> audioSubGroup,
                             juce::URL url,
                             int channelPosition) :
    owner(audioResourceContainer),
    audioTrack(audioTrack),
    audioSubGroup(audioSubGroup),
    url(url)
{
    auto reader = owner.getAudioFormatManager()->createReaderFor (getUrl().getLocalFile());
    audioFormatReader = std::unique_ptr<juce::AudioFormatReader>(reader);
    
    channelMapping = std::make_unique<audium::ChannelMapping>();
    
    if (channelPosition >= 0)
    {
        this->audioTrack->ensureNumChannels(channelPosition + getNumChannels());
        setChannelPosition(channelPosition);
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
    if (device != nullptr)
    {
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
    if (audioFormatReader != nullptr)
    {
        return audioFormatReader->sampleRate;
    }
    return 44100.0;
}

unsigned int AudioResource::getNumChannels() const
{
    if (audioFormatReader != nullptr)
    {
        return audioFormatReader->numChannels;
    }
    
    return numChannels;
}

double AudioResource::getFileLength(audium::TimeContextType context) const
{
    auto length = lengthInSeconds;
 
    if (audioFormatReader != nullptr)
    {
        length = audioFormatReader->lengthInSamples / audioFormatReader->sampleRate;
    }
    
    if (context == audium::seconds)
    {
        return length;
    }
    else if (context == audium::clocks)
    {
        return owner.getTempoProvider()->secondsToClocks(length);
    }
    
    jassertfalse;
    return 0.0;
}

std::vector<std::shared_ptr<AudioResource>> AudioResource::getAudioResourcesWithinSubGroup() const
{
    std::vector<std::shared_ptr<AudioResource>> result;
    auto resources = owner.getAudioResourcesForSubGroup(audioSubGroup.get());
 
    for (auto resource : resources)
    {
        if (resource.get() == this)
            continue;
    
        result.push_back(resource);
    }
    return result;
}

bool AudioResource::containsAbsolutePosition(double position, audium::TimeContextType context) const
{
    auto startTime = getAudioSubGroup()->getAudioClip()->getAbsolutePosition(context);
    auto endTime = startTime + getAudioSubGroup()->getAudioClip()->getRegionData(context).getLength();
    juce::Range<double> absoluteRange(startTime, endTime);
    if (absoluteRange.contains(position))
    {
        return true;
    }

    return false;
}

bool AudioResource::writeToJson (json& output)
{
    output["absolute_file_path"]          = getUrlAsString().toStdString();
    output["relative_file_path"] = getRelativePath(AudiumEngine::projectDirectory).toStdString();
    // TODO: fixme 
    output["gain"]               = 1.0; // getAudioTransportSource()->getGain();
    output["channel_position"]  = getChannelPosition();
    output["number_of_channels"] = getNumChannels();
    output["length_in_seconds"] = getFileLength(audium::seconds);
    
    channelMapping->writeToJson(output);
    
    return true;
}

const juce::URL AudioResource::urlFromJson (json& input)
{
    juce::String filePath;
    
    // relative path is always a local file
    if (input.contains("relative_file_path"))
    {
        juce::String relPath = input["relative_file_path"].template get<std::string>();
        filePath = AudiumEngine::projectDirectory.getChildFile(relPath).getFullPathName();
        if (File(filePath).existsAsFile())
            return URL(File(filePath));
    }
    
    std::cout << "warning: relative path does not exist: " << filePath << std::endl;
    
    if (input.contains("absolute_file_path"))
    {
        filePath = input["absolute_file_path"].template get<std::string>();
        auto file = juce::File::createFileWithoutCheckingPath(filePath);
        if (file.existsAsFile())
            return URL(file);
    }
    
    std::cout << "error: absolute path does not exist: " << filePath << std::endl;
    
    if (input.contains("filename"))
    {
        filePath = input["filename"].template get<std::string>();
    }
    
    return URL(File(filePath));
}

bool AudioResource::readFromJson (json& input, bool rebuild)
{
    if (input.contains("number_of_channels"))
        numChannels = input["number_of_channels"].template get<int>();
    
    if (input.contains("length_in_seconds"))
        lengthInSeconds = input["length_in_seconds"].template get<double>();
    
    
    if (! channelMapping->readFromJson(input, rebuild))
    {
        auto channelPos = 0;
        if (input.contains("channel_position"))
        {
            channelPos = input["channel_position"].template get<int>();
        }
        setChannelPosition(channelPos);
    }
    
    jassert(this->url == urlFromJson(input));

    return true;
}


void AudioResource::setSelected(bool bSelected, bool deselectOthers)
{
    if (deselectOthers)
        owner.deselectAllResources();

    selected = bSelected;
}


bool AudioResource::containsChannelNumber(int channelNumber) const
{
    for (auto i = 0; i < getNumChannels(); i++) {
        if (channelMapping->getRemappedChannel(i) == channelNumber)
            return true;
    }
    
    return false;
}

bool AudioResource::containsChannel(std::shared_ptr<AudioChannel> channel) const
{
    return containsChannelNumber(channel->getChannelNumber());
}

int AudioResource::getChannelPosition() const
{
    return channelMapping->getRemappedChannel(0);
}

void AudioResource::setChannelPosition(int startChannel)
{
    std::cout << "AudioResource::setChannelPosition " << startChannel << std::endl;

    channelMapping->clear();
    for (auto i = 0; i < getNumChannels(); i++)
    {
        channelMapping->setOutputChannelMapping(i, i + startChannel);
    }
}

bool AudioResource::deleteChannel(AudioChannel* channel)
{
    auto chanNumber = channel->getChannelNumber();
    if (containsChannelNumber(chanNumber))
    {
        channelMapping->setOutputChannelMapping(chanNumber, -1);
    }
    
    // returns true in case there is no more mapping
    return !channelMapping->anyOutputMapping();
}

void AudioResource::decrementChannelMapping(int startChannelNumber)
{
    channelMapping->decrementChannelMapping(startChannelNumber);
}

const juce::Array<int> AudioResource::getChannelMapping() const
{
    return channelMapping->getChannelMapping();
}

int AudioResource::getRemappedOutputChannel (int outputChannelIndex) const
{
    return channelMapping->getRemappedChannel(outputChannelIndex);
}

int AudioResource::getSourceChannel (int destChannelIndex) const
{
    return channelMapping->getSourceChannel(destChannelIndex);
}

