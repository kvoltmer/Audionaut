//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/AudioSources/VoiceSourceContainer.h"
#include "Engine/AudioSources/VoiceSource.h"
#include "Engine/ActionMessages.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Analysis/AnalysisWorker.h"
#include "Engine/Core/UserPrompter.h"

using namespace juce;

namespace audium {

AudioResourceContainer::~AudioResourceContainer()
{
    audioResources.clear();

    // a no-op for a scratch container that borrows a live session's directories
    deleteTemporaryProjectDirectory();
}

const juce::File AudioResourceContainer::getAudioFileDirectory(const juce::File projectRoot)
{
    if (projectRoot.getFullPathName().isNotEmpty())
        return juce::File(projectRoot.getFullPathName() + File::getSeparatorString() +
                          "Media" + File::getSeparatorString() +
                          "Audio" + File::getSeparatorString());
    return juce::File();
}

const juce::File AudioResourceContainer::getAudioFileDirectory() const
{
    return getAudioFileDirectory(projectDirectories->getProjectDirectory());
}

const juce::File AudioResourceContainer::getAudioRecordingFile(const juce::File& audioDir,
                                                               const int take,
                                                               const int channel)
{
    jassert(audioDir.exists());
    
    // Format as [Year-Month-Day Hour-Minute-Second]
    auto now = juce::Time::getCurrentTime();
    auto formattedStr = now.formatted ("[%Y-%m-%d_%H-%M-%S]");
    
    auto takeString = juce::String(AudioRegionContainer::formatNumber(take));
    auto channelString = juce::String(AudioRegionContainer::formatNumber(channel + 1));
    
    auto fileName = "take" + takeString + "-chan" + channelString + "-" + formattedStr +".wav";
    
    auto uniqueFile = juce::File(audioDir.getFullPathName() + File::getSeparatorString() + fileName);
    return uniqueFile;
    
}

void AudioResourceContainer::deleteTemporaryProjectDirectory()
{
    // borrowed from a live session: not ours to delete
    if (! projectDirectories->ownsTemporaryDirectory())
        return;

    const auto tempDirectory = projectDirectories->getTemporaryDirectory();
    if (tempDirectory.exists())
        tempDirectory.deleteRecursively();
    projectDirectories->setTemporaryDirectory(File());
}

bool AudioResourceContainer::createTemporaryProjectDirectory(bool reset)
{
    if (reset)
        deleteTemporaryProjectDirectory();

    // A directory that is set but gone (the OS purged its cache, a borrowed
    // live session's dir) is made again at its path: the resources whose
    // relative paths point there, and the live session, stay valid. Only a
    // never-set one gets a name.
    if (projectDirectories->getTemporaryDirectory() == File()) {
        // use a unique directory within the temp location
        // example: ~/Library/Containers/com.voltmer-systems.audionaut/Data/Library/Caches/Audionaut/temp-50a181e5/Media/Audio
        auto uniqueName = "temp-" + String::toHexString (Random::getSystemRandom().nextInt()) + ProjectFileStore::projectFileExtension;
        const auto tempDirectory = File(File::getSpecialLocation(File::tempDirectory).getFullPathName() +
                                        File::getSeparatorString() +
                                        uniqueName);
        // make sure the directory is unique!
        jassert(!tempDirectory.exists());
        projectDirectories->setTemporaryDirectory(tempDirectory);
    }
    
    auto audioDirectory = getAudioFileDirectory(projectDirectories->getTemporaryDirectory());
    
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

bool AudioResourceContainer::copyAudioFiles(const juce::File sourceDirectory,
                                            const juce::File destinationDirectory,
                                            juce::Array<juce::File>& copiedFiles)
{
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

                // always copy, never move: the source stays intact until the
                // caller has committed the save, and a failure part-way can
                // be rolled back by deleting `copiedFiles`
                if (!found.copyFileTo(destinationFile)) {
                    destinationFile.deleteFile(); // a partial copy
                    return false;
                }
                copiedFiles.add(destinationFile);
            }
        }
    }
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
    auto audioDir = getAudioFileDirectory(projectDirectories->getProjectDirectory());
    if ( !audioDir.exists()) {
        
        // use temp dir
        createTemporaryProjectDirectory(false);
        
        audioDir = getAudioFileDirectory(projectDirectories->getTemporaryDirectory());
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
            if ((*it).second->audioFormatReader != nullptr)
                return (*it).second;
        }
    }
    return nullptr;
}

std::shared_ptr<juce::AudioFormatReader> AudioResourceContainer::getAudioFormatReaderForUrl(juce::URL &url)
{
    if (auto audioFormat = formatManager->findFormatForFileExtension(url.getLocalFile().getFileExtension())) {
        
        // file must be a child of our Media/Audio directory
        // see: copyToAudioFileDirectoryIfNeeded(url)
        auto audioDir = getCurrentAudioFileDirectory();
        jassert(url.getLocalFile().isAChildOf(audioDir));
        
        if (auto existingResource = findResourceWithUrl(url)) {
            return existingResource->audioFormatReader;
        }

        auto reader = std::shared_ptr<juce::AudioFormatReader>(audioFormat->createMemoryMappedReader(url.getLocalFile()));
        if (reader != nullptr) {
            if (auto memReader = dynamic_cast<juce::MemoryMappedAudioFormatReader*>(reader.get())) {
                memReader->mapEntireFile();
            }
            return reader;
        }

        return openStreamingReader(url);
    }
    return nullptr;
}

std::shared_ptr<juce::AudioFormatReader> AudioResourceContainer::openStreamingReader(const juce::URL& url) const
{
    if (auto audioFormat = formatManager->findFormatForFileExtension(url.getLocalFile().getFileExtension())) {
        if (auto stream = rawToUniquePtr (juce::URLInputSource(url).createInputStream())) {
            return std::shared_ptr<juce::AudioFormatReader>(audioFormat->createReaderFor(stream.release(), true));
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
    
    auto audioResource = std::make_shared<AudioResource>(*this,
                                                         track,
                                                         resourceGroup,
                                                         url,
                                                         audioFormatReader,
                                                         destChannel,
                                                         sourceChannel);
    if (audioResource != nullptr) {
        audioResources.push_back({track, audioResource});

        // Kick off background analysis of the new file. Recording placeholders
        // (no reader yet) are analysed later, once recording finishes.
        if (analysisWorker != nullptr && ! audioResource->isRecording())
            analysisWorker->enqueue(juce::File(audioResource->getFullPathName()));

        return audioResource;
    }

    return nullptr;
}

std::shared_ptr<VoiceSource> AudioResourceContainer::createTransportSourceForAudioResource(std::shared_ptr<AudioResource> audioResource)
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
            auto sources = voiceSourceContainer->getVoiceSourcesForResource(*resource.get());
            for (auto source : sources) {
                voiceSourceContainer->removeVoiceSource(source);
            }
            audioResources.erase(it);
            cancelAnalysisIfUnloaded(juce::File(resource->getFullPathName()));
            break;
        }
    }
}

void AudioResourceContainer::removeAudioResourcesForTrack (AudioTrack *track)
{
    jassert(track != nullptr);
    if (track != nullptr) {
        std::vector<juce::File> removedFiles;
        for (auto it = audioResources.begin(); it != audioResources.end();) {
            if ((*it).first.get() == track) {
                removedFiles.push_back(juce::File((*it).second->getFullPathName()));
                it = audioResources.erase(it);
            }
            else {
                ++it;
            }
        }
        for (const auto& file : removedFiles)
            cancelAnalysisIfUnloaded(file);
    }
}

void AudioResourceContainer::cancelAnalysisIfUnloaded(const juce::File& audioFile)
{
    // Another resource may still reference the same file - only cancel once
    // nothing loaded uses it anymore.
    if (analysisWorker != nullptr && ! isAudioFileCurrentlyLoaded(audioFile))
        analysisWorker->cancel(audioFile);
}

int AudioResourceContainer::getNumAudioResources() const
{
    return static_cast<int>(audioResources.size());
}

void AudioResourceContainer::cleanup()
{
    audioResources.clear();

    // No resources are loaded anymore, so any analysis still queued is for
    // unloaded files - drop it all.
    if (analysisWorker != nullptr)
        analysisWorker->cancelAll();
}

std::vector<juce::File> AudioResourceContainer::findObsoleteAudioFiles(const json &json)
{
    std::vector<juce::File> redundantFiles;
    std::vector<std::string> jsonPaths;
    
    if (!json.empty()) {
        
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

        const auto projectDirectory = projectDirectories->getProjectDirectory();
        auto audioDir = getAudioFileDirectory(projectDirectory);
        for (auto& found : audioDir.findChildFiles (File::findFiles, false, "*")) {
            
            auto relPath = found.getRelativePathFrom(projectDirectory);
            if (std::find(jsonPaths.begin(), jsonPaths.end(), relPath) == jsonPaths.end()) {
                redundantFiles.push_back(found);
            }
        }
    }

    return redundantFiles;
}

void AudioResourceContainer::deleteObsoleteAudioFiles(const juce::File projectDirectory)
{
    // consitency check
    std::vector<juce::File> redundantFiles;
    for (auto& found : projectDirectory.findChildFiles (File::findFiles, false, "*")) {
        if (!isAudioFileCurrentlyLoaded(found)) {
            redundantFiles.push_back(found);
        }
    }
    
    trashRedundantFiles(redundantFiles);
}

void AudioResourceContainer::trashRedundantFiles(const std::vector<juce::File>& redundantFiles)
{
    if (redundantFiles.empty())
        return;

    // Headless sessions (CLI, MCP, in-app CLI mode) never delete: their
    // prompter declines (there is nobody to ask, and the remove-track /
    // remove-channel verbs promise that the audio files stay in the
    // package). Cleaning up is a GUI decision, taken through the GUI's
    // prompter.
    String redundantFilesString;
    for (size_t i = 0; i < redundantFiles.size(); i++) {
        redundantFilesString += redundantFiles[i].getFullPathName() + "\n";

        // don't display more than 10 Files
        if (i > 10) {
            redundantFilesString += "etc...";
            break;
        }
    }

    const auto confirmed = userPrompter->confirm("Redundant files found. Move files to trash?",
                                                 "The following audio files are not used in the project anymore:\n\n" +
                                                 redundantFilesString +
                                                 "\nDo you want to move " + String(redundantFiles.size()) + " files to trash?");
    if (! confirmed)
        return;

    for (auto& file : redundantFiles) {
        if (! file.moveToTrash()) {
            userPrompter->showMessage("Error", "Moving files to trash failed.");
            return;
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

void AudioResourceContainer::onRecordingFinished()
{
    // load recorded audio files
    for (auto &itr : audioResources) {
        auto resource = itr.second;

        // A successful load means a freshly recorded file is now on disk with a
        // reader - analyse it in the background, like any newly added resource.
        if (resource->loadRecordedAudioFile()
            && analysisWorker != nullptr)
        {
            analysisWorker->enqueue(juce::File(resource->getFullPathName()));
        }
    }
}

std::shared_ptr<AudioThumbnail> AudioResourceContainer::getThumbnailForResource(const std::shared_ptr<AudioResource>& resource)
{
    if (resource == nullptr || resource->audioFormatReader == nullptr)
        return nullptr;

    // the map holds a weak reference per file ever asked for; drop the ones
    // whose thumbnail is gone so it does not grow for the life of the engine
    std::erase_if(sharedThumbnails, [](const auto& entry) { return entry.second.expired(); });

    const auto key = resource->getUrl().toString(true);
    if (auto found = sharedThumbnails.find(key); found != sharedThumbnails.end())
        return found->second.lock();

    constexpr int sourceSamplesPerThumbnailSample = 64;
    auto thumbnail = std::make_shared<AudioThumbnail>(sourceSamplesPerThumbnailSample, *formatManager, *audioThumbnailCache);
    sharedThumbnails[key] = thumbnail;

    // a memory-mapped reader is shared with the thumbnail, anything else gets its own input source
    if (dynamic_cast<MemoryMappedAudioFormatReader*>(resource->audioFormatReader.get()) != nullptr)
        thumbnail->setReader(resource->audioFormatReader, key.hashCode64());
    else
        thumbnail->setSource(new juce::URLInputSource(resource->getUrl()));

    return thumbnail;
}

} // namespace audium
