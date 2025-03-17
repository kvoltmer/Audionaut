//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <memory>
#include <JuceHeader.h>

#include "Engine/Streamable.h"
#include "Engine/TimeContext.h"
#include "Engine/Export/ExportAudioConfig.h"

class AudioTrackContainer;
class AudioTrack;
class PlayListContainer;
class AudioRegionContainer;
class AudioResourceContainer;
class TransportSourceContainer;
class PlayListScheduler;
struct AutoEditConfig;
class LinkAudioDevice;
namespace audium {
    class AudioBusInterface;
}

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
                 std::shared_ptr<audium::AudioBusInterface> audioBusInterface_) :
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
    std::shared_ptr<audium::AudioBusInterface> getAudioBusInterface() const { return audioBusInterface; }
    
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
    std::shared_ptr<audium::AudioBusInterface> audioBusInterface;
    
    juce::File currentFile;

    json uiState;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumEngine)
};
