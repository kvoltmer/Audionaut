//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <JuceHeader.h>

#include "Engine/Streamable.h"
#include "Engine/TimeContext.h"
#include "Engine/Export/ExportAudioConfig.h"

namespace audium {

class AudioTrackContainer;
class AudioTrack;
class PlayListContainer;
class AudioRegionContainer;
class AudioResourceContainer;
class TransportSourceContainer;
class PlayListScheduler;
struct AutoEditConfig;
class LinkAudioDevice;
class AudioBusInterface;


/// The Audium engine
class AudiumEngine : public audium::Streamable
{
    
public:
    AudiumEngine(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager_,
                 std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
                 std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                 std::shared_ptr<PlayListScheduler> playListScheduler_,
                 std::shared_ptr<LinkAudioDevice> linkAudioDevice_,
                 std::shared_ptr<juce::UndoManager> undoManager_,
                 std::shared_ptr<AudioBusInterface> audioBusInterface_) :
    audioDeviceManager(audioDeviceManager_),
    audioTrackContainer(audioTrackContainer_),
    audioResourceContainer(audioResourceContainer_),
    playListScheduler(playListScheduler_),
    linkAudioDevice(linkAudioDevice_),
    undoManager(undoManager_),
    audioBusInterface(audioBusInterface_)
    {
    }
    
    ~AudiumEngine();
    
    void initialise();
    void uninitialise();
    void cleanup();
    void createNewProject();
    
    void openFile (const juce::File& file, std::function<void (bool,std::string)> callback);
    void saveFile (const juce::File& file, std::function<void (bool,std::string)> callback);
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream, bool rebuild) override;
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    int getSizeInUnits() override;
    
    void createDefaultRegionAndPlayList(std::shared_ptr<AudioTrack> track);
    
    static const char* projectFileExtension;
    
    static juce::File projectDirectory;
    
    const juce::File getCurrentFile() const { return currentFile; }
    
    std::shared_ptr<AudioTrackContainer> getAudioTrackContainer() const { return audioTrackContainer; }
    std::shared_ptr<AudioResourceContainer> getAudioResourceContainer() const { return audioResourceContainer; }
    std::shared_ptr<PlayListContainer> getPlayListContainer(std::shared_ptr<AudioTrack> track) const;
    std::shared_ptr<PlayListScheduler> getPlayListScheduler() const { return playListScheduler; }
    std::shared_ptr<juce::UndoManager> getUndoManager() const { return undoManager; }
    std::shared_ptr<juce::AudioDeviceManager> getAudioDeviceManager() const { return audioDeviceManager; }
    std::shared_ptr<AudioBusInterface> getAudioBusInterface() const { return audioBusInterface; }
    
    void invokeAutoEdit(const AutoEditConfig config);
    
    json& getUiState() { return uiState; }
    
    void setBypass(bool bypass);
    
private:
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;
    std::shared_ptr<AudioTrackContainer> audioTrackContainer;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<PlayListScheduler> playListScheduler;
    std::shared_ptr<LinkAudioDevice> linkAudioDevice;
    std::shared_ptr<juce::UndoManager> undoManager;
    std::shared_ptr<AudioBusInterface> audioBusInterface;
    
    juce::File currentFile;
    
    json uiState;
        
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumEngine)
};

} // namespace audium
