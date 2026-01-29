//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/ActionMessages.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Factory/AudioResourceFactory.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/Channel/AudioChannel.h"

using namespace juce;

namespace audium {

AudioResourceContainer::~AudioResourceContainer()
{
    audioResources.clear();
    deleteTemporaryProjectDirectory();
}

const juce::File AudioResourceContainer::getAudioFileDirectory(const juce::File projectRoot)
{
    return juce::File(projectRoot.getFullPathName() + File::getSeparatorString() +
                      "Media" + File::getSeparatorString() +
                      "Audio" + File::getSeparatorString());
}

const juce::File AudioResourceContainer::getAudioFileDirectory()
{
    return getAudioFileDirectory(AudiumEngine::projectDirectory);
}

const juce::File AudioResourceContainer::getAudioRecordingFile(const int take,
                                                               const int channel)
{
    auto audioDir = getAudioFileDirectory(AudiumEngine::projectDirectory);
    if (!audioDir.exists()) {
        audioDir = getAudioFileDirectory(AudiumEngine::tempDirectory);
    }
    jassert(audioDir.exists());
    
    // Format as [Year-Month-Day Hour-Minute-Second]
    auto now = juce::Time::getCurrentTime();
    auto formattedStr = now.formatted ("[%Y-%m-%d_%H-%M-%S]");
    
    auto takeString = juce::String(AudioRegionContainer::formatNumber(take + 1));
    auto channelString = juce::String(AudioRegionContainer::formatNumber(channel + 1));
    
    auto fileName = "take" + takeString + "-chan" + channelString + "-" + formattedStr +".wav";
    
    auto uniqueFile = juce::File(audioDir.getFullPathName() + File::getSeparatorString() + fileName);
    return uniqueFile;
    
}

void AudioResourceContainer::deleteTemporaryProjectDirectory()
{
    if (AudiumEngine::tempDirectory.exists())
        AudiumEngine::tempDirectory.deleteRecursively();
    AudiumEngine::tempDirectory = File();
}

bool AudioResourceContainer::createTemporaryProjectDirectory(bool reset)
{
    if (reset)
        deleteTemporaryProjectDirectory();
    
    if (!AudiumEngine::tempDirectory.exists()) {
        // use a unique directory within the temp location
        // example: ~/Library/Containers/com.voltmer-systems.audionaut/Data/Library/Caches/Audionaut/temp-50a181e5/Media/Audio
        auto uniqueName = "temp-" + String::toHexString (Random::getSystemRandom().nextInt()) + AudiumEngine::projectFileExtension;
        AudiumEngine::tempDirectory = File(File::getSpecialLocation(File::tempDirectory).getFullPathName() +
                                                    File::getSeparatorString() +
                                                    uniqueName);
        // make sure the directory is unique!
        jassert(!AudiumEngine::tempDirectory.exists());
    }
    
    auto audioDirectory = getAudioFileDirectory(AudiumEngine::tempDirectory);
    
    if (!audioDirectory.exists()) {
        auto success = audioDirectory.createDirectory();
        std::cout << "create: " << audioDirectory.getFullPathName() << std::endl;
        if (!success) {
            jassertfalse;
            return false;
        }
    }
    
    return true;
}

bool AudioResourceContainer::isAudioFileCurrentlyLoaded(const juce::File audioFile) const
{
    for (auto& iter : audioResources) {
        if (iter.second->getUrl().getLocalFile() == audioFile) {
            return true;
        }
    }
    return false;
}

bool AudioResourceContainer::copyOrMoveAudioFiles(const juce::File sourceDirectory,
                                                  const juce::File destinationDirectory)
{
    juce::String debugString;
    if (sourceDirectory != destinationDirectory) {
            
        if (!destinationDirectory.exists()) {
            if (!destinationDirectory.createDirectory())
                return false;
        }
        for (auto& found : sourceDirectory.findChildFiles (File::findFiles, false, "*")) {
            
            if (!isAudioFileCurrentlyLoaded(found)) {
                std::cout << "SKIP : " << found.getFileName() << std::endl;
                continue;
            }
            
            auto destinationFile = File(destinationDirectory.getFullPathName() + File::getSeparatorString() + found.getFileName());
            if (!destinationFile.existsAsFile()) {
                
                if (found.isAChildOf(File::getSpecialLocation(File::tempDirectory))) {
                    if (!found.moveFileTo(destinationFile))
                        return false;
                    debugString += "moved to: " + destinationFile.getFileName() + "\n";
                }
                else {
                    if (!found.copyFileTo(destinationFile))
                        return false;
                    debugString += "copied to: " + destinationFile.getFileName() + "\n";
                }
            }
        }        
    }
#if _DEBUG
#if !defined(CATCH2_TESTS)
    if (debugString.isNotEmpty()) {
        
        auto messageString = "Destination: " + destinationDirectory.getFullPathName() + "\n\n";
        messageString += debugString;
        NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                              "copyOrMoveAudioFiles",
                                              messageString);
    }
#endif
#endif
    return true;
}

void AudioResourceContainer::changeAudioFilePaths(const juce::File newPath)
{
    for (auto &itr : audioResources) {
        auto resource = itr.second;
        auto fileName = resource->getUrl().getLocalFile().getFileName();
        auto newFile = File(newPath.getFullPathName() + File::getSeparatorString() + fileName);
        jassert(newFile.existsAsFile());
        resource->setUrl(URL(newFile));
    }
}

juce::File AudioResourceContainer::getCurrentAudioFileDirectory()
{
    // try to use project directory
    auto audioDir = getAudioFileDirectory(AudiumEngine::projectDirectory);
    if ( !audioDir.exists()) {
        
        // use temp dir
        createTemporaryProjectDirectory(false);
        
        audioDir = getAudioFileDirectory(AudiumEngine::tempDirectory);
        jassert(audioDir.exists());
    }
    
    return audioDir;
}


bool AudioResourceContainer::copyToAudioFileDirectoryIfNeeded(juce::URL &url)
{
    if (formatManager->findFormatForFileExtension(url.getLocalFile().getFileExtension()) != nullptr) {
        
        auto externalFile = url.getLocalFile();
        jassert(externalFile.existsAsFile());
        
        auto audioDir = getCurrentAudioFileDirectory();
        
        if ( !externalFile.isAChildOf(audioDir)) {
            
            auto audioFile = File(audioDir.getFullPathName() + File::getSeparatorString() + url.getLocalFile().getFileName());
            if ( !audioFile.existsAsFile()) {
                // !!! copy audio file !!!
                bool success = externalFile.copyFileTo(audioFile);
                jassert(success);
                std::cout << "file copied to: " << audioFile.getFullPathName() << std::endl;
                url = juce::URL(audioFile);
                return true;
            }
            else {
                
                for (auto& found : audioDir.findChildFiles (File::findFiles, false, externalFile.getFileNameWithoutExtension() + "*")) {
                    if (found.hasIdenticalContentTo(externalFile)) {
                        // duplicate with identical content was found
                        url = URL(found);
                        if (isAudioFileCurrentlyLoaded(found)) {
                            return false;
                        }
                        else {
                            // pretend the file was copied.
                            return true;
                        }
                    }
                }
                // the file already exists (in terms of file name) but they aren't the same file in terms of content...
                // copy the imported file but with different name:
                auto newAudioFile = audioFile.getNonexistentSibling();
                bool success = externalFile.copyFileTo(newAudioFile);
                jassert(success);
                std::cout << "file copied to: " << newAudioFile.getFullPathName() << std::endl;
                url = juce::URL(newAudioFile);
                return true;
            }
        }
    }
    return false;
}

std::shared_ptr<AudioResource> AudioResourceContainer::findResourceWithUrl(juce::URL url) const
{
    for (auto it = audioResources.begin(); it != audioResources.end(); ++it) {
        if ((*it).second->getUrl() == url) {
            //            std::cout << "found url: " << url.getFileName() << std::endl;
            if ((*it).second->audioFormatReader != nullptr)
                return (*it).second;
        }
    }
    return nullptr;
}

std::shared_ptr<juce::AudioFormatReader> AudioResourceContainer::getAudioFormatReaderForUrl(juce::URL &url)
{
    if (formatManager->findFormatForFileExtension(url.getLocalFile().getFileExtension()) != nullptr) {
        
        // file must be a child of our Media/Audio directory
        // see: copyToAudioFileDirectoryIfNeeded(url)
        auto audioDir = getCurrentAudioFileDirectory();
        jassert(url.getLocalFile().isAChildOf(audioDir));
        
        if (auto existingResource = findResourceWithUrl(url)) {
            return existingResource->audioFormatReader;
        }
        else {
            return AudioResourceFactory::createAudioFormatReader(url, *formatManager.get());
        }
    }
    return nullptr;
}




std::shared_ptr<AudioResource> AudioResourceContainer::addAudioResource (juce::URL url,
                                                                         std::shared_ptr<juce::AudioFormatReader> audioFormatReader,
                                                                         std::shared_ptr<AudioTrack> track,
                                                                         std::shared_ptr<ResourceGroup> resourceGroup,
                                                                         int destChannel,
                                                                         int sourceChannel)
{
    jassert(track != nullptr);
    jassert(resourceGroup != nullptr);
    
    auto audioResource = AudioResourceFactory::createAudioResource(url,
                                                                   audioFormatReader,
                                                                   *this,
                                                                   track,
                                                                   resourceGroup,
                                                                   destChannel,
                                                                   sourceChannel);
    if (audioResource != nullptr) {
        audioResources.push_back({track, audioResource});
        return audioResource;
    }
    
    return nullptr;
}

std::shared_ptr<AudiumTransportSource> AudioResourceContainer::createTransportSourceForAudioResource(std::shared_ptr<AudioResource> audioResource)
{
    if (audioResource->audioFormatReader != nullptr) {
        auto source = std::make_shared<AudioFormatReaderSource>(audioResource->audioFormatReader.get(), false);
        if (source != nullptr && source->getAudioFormatReader() != nullptr) {
            return audioResource->createNewTransportSource(source);
        }
    }
    return nullptr;
}

void AudioResourceContainer::removeAudioResource(std::shared_ptr<AudioResource> resource)
{
    for (auto it = audioResources.begin(); it != audioResources.end(); ++it) {
        if ((*it).second == resource) {
            auto sources = transportSourceContainer->getTransportSourcesForResource(*resource.get());
            for (auto source : sources) {
                transportSourceContainer->removeTransportSource(source);
            }
            audioResources.erase(it);
            break;
        }
    }
}

void AudioResourceContainer::removeAudioResourcesForTrack (AudioTrack *track)
{
    jassert(track != nullptr);
    if (track != nullptr) {
        for (auto it = audioResources.begin(); it != audioResources.end(); it++) {
            if ((*it).first.get() == track)
                audioResources.erase(it++);
        }
    }
}

int AudioResourceContainer::getNumAudioResources() const
{
    return static_cast<int>(audioResources.size());
}

void AudioResourceContainer::cleanup()
{
    audioResources.clear();
}

void AudioResourceContainer::deleteObsoleteAudioFiles(const json &json)
{
    
    std::vector<std::string> jsonPaths;
    
    if (!json.empty()) {
        // std::cout << std::setw(2) << json << std::endl;
        
        if (json.contains("audium")) {
            for (auto track : json["audium"]["audio_tracks"]) {
                for (auto resGroup : track["resource_groups"]) {
                    for (auto res : resGroup["resources"]) {
                        if (res.contains("relative_file_path")) {
                            auto relPath = res["relative_file_path"].template get<std::string>();
                            jsonPaths.push_back(relPath);
                        }
                    }
                }
            }
        }
        
        if (jsonPaths.size() > 0) {
            auto audioDir = getAudioFileDirectory(AudiumEngine::projectDirectory);
            for (auto& found : audioDir.findChildFiles (File::findFiles, false, "*")) {
                
                auto relPath = found.getRelativePathFrom(AudiumEngine::projectDirectory);
                if (std::find(jsonPaths.begin(), jsonPaths.end(), relPath) == jsonPaths.end()) {
                    if (found.deleteFile()) {
                        std::cout << "obsolete file deleted: " << found.getFullPathName() << std::endl;
                    }
                }
            }
        }
    }
}

std::vector<std::shared_ptr<AudioResource>> AudioResourceContainer::getAudioResourcesForTrack(AudioTrack *track) const
{
    std::vector<std::shared_ptr<AudioResource>> result;
    for (auto itr = audioResources.begin(); itr != audioResources.end(); itr++) {
        if (itr->first.get() == track) {
            result.push_back(itr->second);
        }
    }
    return result;
}

std::vector<std::shared_ptr<AudioResource>> AudioResourceContainer::getAudioResourcesForResourceGroup(const ResourceGroup *resourceGroup) const
{
    std::vector<std::shared_ptr<AudioResource>> result;
    for (auto itr = audioResources.begin(); itr != audioResources.end(); itr++) {
        if (itr->second->getResourceGroup().get() == resourceGroup) {
            jassert(itr->first.get() == &resourceGroup->getAudioTrack());
            result.push_back(itr->second);
        }
    }
    return result;
}

std::vector<std::shared_ptr<AudioResource>> AudioResourceContainer::getAudioResourcesForChannel(const AudioChannel *channel) const
{
    std::vector<std::shared_ptr<AudioResource>> result;
    for (const auto &itr : audioResources) {
        
        if (itr.first.get() == &channel->getAudioTrack()) {
            auto resource = itr.second;
            auto destChannel = resource->getChannelMapping().getDestinationChannel();
            if (destChannel == channel->getChannelNumber()) {
                result.push_back(resource);
            }
        }
    }
    
    return result;
}

std::shared_ptr<AudioTrack> AudioResourceContainer::getAudioTrackForResource(std::shared_ptr<AudioResource> resource) const
{
    for (auto itr = audioResources.begin(); itr != audioResources.end(); itr++)
    {
        if (itr->second == resource)
        {
            return itr->first;
        }
    }
    return nullptr;
    
}

std::vector<std::shared_ptr<AudioTrack>> AudioResourceContainer::getAudioTracks() const
{
    std::vector<std::shared_ptr<AudioTrack>> result;
    for(auto it = audioResources.begin(), end = audioResources.end(); it != end; it++)
    {
        if (std::find(result.begin(), result.end(), it->first) == result.end())
        {
            result.push_back(it->first);
        }
    }
    return result;
}

void AudioResourceContainer::onDeleteChannel(AudioTrack* audioTrack, AudioChannel* channel)
{
    std::vector<std::shared_ptr<AudioResource>> resourcesToRemove;
    
    for (auto it = audioResources.begin(), end = audioResources.end(); it != end; it++)
    {
        if (it->first.get() == audioTrack)
        {
            if (it->second->getChannelMapping().deleteChannel(channel->getChannelNumber()))
            {
                // no more channel mapping -> remove
                resourcesToRemove.push_back(it->second);
            }
        }
    }
    
    for (auto resource : resourcesToRemove)
    {
        removeAudioResource(resource);
    }
}

void AudioResourceContainer::updateRecordingLength(double length, audium::TimeContextType context)
{
    for (auto &itr : audioResources)
        itr.second->updateRecordingLength(length, context);
}

void AudioResourceContainer::onRecordingFinished()
{
    // load recorded audio files
    for (auto &itr : audioResources) {
        itr.second->loadRecordedAudioFile();
    }
}

} // namespace audium
