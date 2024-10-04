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
#include "Engine/TransportSourceContainer.h"
#include "Engine/AudiumTransportSource.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/Factory/AudioResourceFactory.h"

AudioResource::AudioResource(AudioResourceContainer& audioResourceContainer,
                             std::shared_ptr<AudioGroup> audioGroup,
                             std::shared_ptr<AudioSubGroup> audioSubGroup,
                             juce::URL url,
                             int channelPosition) :
    owner(audioResourceContainer),
    audioGroup(audioGroup),
    audioSubGroup(audioSubGroup),
    url(url)
{
    auto reader = owner.getAudioFormatManager()->createReaderFor (getUrl().getLocalFile());
    audioFormatReader = std::unique_ptr<juce::AudioFormatReader>(reader);
    
    if (channelPosition >= 0)
    {
        auto numChannels = getNumChannels();
        this->audioGroup->ensureNumChannels(channelPosition + numChannels);
        setChannelPosition(channelPosition);
    }
}


AudioResource::~AudioResource()
{
    audioChannels.clear();
}

std::shared_ptr<AudiumTransportSource> AudioResource::createNewTransportSource(juce::TimeSliceThread* readAheadThread,
                                                                               std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource)
{
    auto readAheadBufferSize = 48000;
    
    
    auto memReader = dynamic_cast<MemoryMappedAudioFormatReader*>(audioFormatReaderSource->getAudioFormatReader());
    if (memReader)
    {
        readAheadBufferSize = 0;
        readAheadThread = nullptr;
    }
            
    
    auto transportSource = audioGroup->getTransportSourceContainer()->createAndAddTransportSource(audioFormatReaderSource);
    transportSource->setSource (audioFormatReaderSource.get(),
                                readAheadBufferSize,
                                readAheadThread,
                                audioFormatReaderSource->getAudioFormatReader()->sampleRate);
 
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
    const auto gain         = input["gain"].template get<float>();
    const auto channelPos   = input["channel_position"].template get<int>();
    
    if (input.contains("number_of_channels"))
        numChannels = input["number_of_channels"].template get<int>();
    
    if (input.contains("length_in_seconds"))
        lengthInSeconds = input["length_in_seconds"].template get<double>();
    
    setChannelPosition(channelPos);
    jassert(this->url == urlFromJson(input));
    
    // TODO: fixme
    //if (getAudioTransportSource() != nullptr)
    //    getAudioTransportSource()->setGain(gain);
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
    juce::Range<int> channelRange(getChannelPosition(),
                                  getChannelPosition() + getNumChannels());
    
    if (channelRange.contains(channelNumber))
    {
        return true;
    }
    return false;
}

bool AudioResource::containsChannel(std::shared_ptr<AudioChannel> channel) const
{
    auto it = std::find(audioChannels.begin(), audioChannels.end(), channel);
    if (it != audioChannels.end())
    {
        return true;
    }
    
    return false;
}

int AudioResource::getChannelPosition() const
{
    if (audioChannels.size() > 0)
    {
        return audioChannels[0]->getChannelNumber();
    }
    
    return 0;
}

void AudioResource::setChannelPosition(int startChannel)
{
    //std::cout << "AudioResource::setChannelPosition " << startChannel << std::endl;

    audioChannels.clear();

    for (auto i = 0; i < getNumChannels(); i++)
    {
        auto channel = audioGroup->getChannel(i + startChannel);
        jassert(channel);
        if (channel != nullptr)
        {
            audioChannels.push_back(channel);
        }
    }
    
    jassert(audioChannels.size() == getNumChannels());
}

bool AudioResource::deleteChannel(AudioChannel* channel)
{
    auto it = std::find_if(audioChannels.begin(), audioChannels.end(), [channel](const auto& item) {
        return item.get() == channel;
    });
    if (it != audioChannels.end())
    {
        audioChannels.erase(it);
    }
    
    return audioChannels.size() == 0;
}
