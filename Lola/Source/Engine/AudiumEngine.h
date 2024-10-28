/*
  ==============================================================================

    AudiumEngine.h
    Created: 29 Jan 2023 12:32:40pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <memory>
#include <JuceHeader.h>

#include "Engine/Streamable.h"
#include "Engine/TimeContext.h"
#include "Engine/ExportAudioConfig.h"

class AudioTrackContainer;
class AudioTrack;
class PlayListContainer;
class AudioRegionContainer;
class AudioResourceContainer;
class TransportSourceContainer;
class PlayListScheduler;
struct AutoEditConfig;
class LinkAudioDevice;

/// The Audium engine
class AudiumEngine : public audium::Streamable
{
    
public:
    AudiumEngine(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                 std::shared_ptr<AudioTrackContainer> audioTrackContainer,
                 std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                 std::shared_ptr<PlayListScheduler> playListScheduler,
                 std::shared_ptr<LinkAudioDevice> linkAudioDevice,
                 std::shared_ptr<juce::UndoManager> undoManager) :
        audioDeviceManager(audioDeviceManager),
        audioTrackContainer(audioTrackContainer),
        audioResourceContainer(audioResourceContainer),
        playListScheduler(playListScheduler),
        linkAudioDevice(linkAudioDevice),
        undoManager(undoManager)
    {
    }
    
    ~AudiumEngine();
    
    void initialise();
    void uninitialise();
    void cleanup();
    
    void openFile (const juce::File& file, std::function<void (bool,std::string)> callback);
    bool saveFile (const juce::File& file);
    void bounceToFile(const audium::ExportAudioConfig config);
    
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

    void invokeAutoEdit(const AutoEditConfig config);
    
    json& getUiState() { return uiState; }
    
private:
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;
    std::shared_ptr<AudioTrackContainer> audioTrackContainer;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<PlayListScheduler> playListScheduler;
    std::shared_ptr<LinkAudioDevice> linkAudioDevice;
    std::shared_ptr<juce::UndoManager> undoManager;
    
    
    juce::File currentFile;

    json uiState;
    

    
    void setBypass(bool bypass);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumEngine)
};
