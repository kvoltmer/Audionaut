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

#include <JuceHeader.h>
#include "Engine/Streamable.h"
#include "Engine/TimeContext.h"
#include "Engine/Region/AudioRegionData.h"
#include "Engine/Group/AudioRegionAdapter.h"
#include "Engine/Selection/SelectionManager.h"


class AudioTrack;
class AudioResourceContainer;
class AudioRegionContainer;
class AudiumEngine;
class TempoProvider;
class AudioRegion;
class TransportSourceContainer;
class AudioResourceContainer;


namespace audium {
    class AudioBusInterface;
    class TransportLoop;
}

class AudioTrackContainer : public juce::ActionBroadcaster,
                            public juce::ChangeBroadcaster,
                            public audium::Streamable
{
        
public:
    
    AudioTrackContainer(std::shared_ptr<juce::UndoManager> undoManager,
                        std::shared_ptr<TempoProvider> tempoProvider,
                        std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                        std::shared_ptr<TransportSourceContainer> transportSourceContainer,
                        std::shared_ptr<audium::SelectionManager> selectionManager,
                        std::shared_ptr<audium::AudioBusInterface> audioBusInterface_,
                        std::shared_ptr<audium::TransportLoop> transportLoop_) :
        audioBusInterface(audioBusInterface_),
        undoManager(undoManager),
        tempoProvider(tempoProvider),
        audioResourceContainer(audioResourceContainer),
        transportSourceContainer(transportSourceContainer),
        selectionManager(selectionManager),
        transportLoop(transportLoop_),
        audioRegionAdapter(*this)
    {
    }
    
    ~AudioTrackContainer();
    
    void setMasterGain(const float newGain);
    const float getMasterGain() const noexcept;
    
    bool groupIdExists(const int groupId) const;
        
    std::shared_ptr<AudioTrack> createNewAudioTrack(const juce::String nameString);
    void cleanup();
    
    bool deleteAudioTrack(AudioTrack* track);
    bool deleteAudioTrack(std::shared_ptr<AudioTrack> track);
    void deleteSelectedObjects();
    void deleteUnusedRegions();
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream, bool rebuild) override;
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    int getSizeInUnits() override;
    
    std::shared_ptr<AudioTrack> getSelectedGroup() const { return audioTracks[selectedGroup]; }
    
    int getNumItems() const { return static_cast<int>(audioTracks.size());}
    std::shared_ptr<AudioTrack> getAudioTrack(int index) const;
    int getAudioTrackId(std::shared_ptr<const AudioTrack> searchTrack) const;
    int getChannelOffset(std::shared_ptr<const AudioTrack> searchTrack) const;
    
    std::shared_ptr<AudioTrack> getDefaultGroup() const;
    
    const std::vector<std::shared_ptr<AudioTrack>> &getAudioTracks() const { return audioTracks; }
        
    void selectAllGroups(bool bSelected, bool selectChildren);
    juce::SparseSet<int> getSelectedRows() const;
    void setSelectedRows(juce::SparseSet<int>& selectedRows);

    std::shared_ptr<TempoProvider> getTempoProvider() const noexcept { return tempoProvider; }
    std::shared_ptr<juce::UndoManager> getUndoManager() const noexcept { return undoManager; }
    std::shared_ptr<TransportSourceContainer> getTransportSourceContainer() const noexcept { return transportSourceContainer; }
    std::shared_ptr<audium::SelectionManager> getSelectionManager() const noexcept { return selectionManager; }
    
    AudioRegionAdapter &getAudioRegionAdapter() { return audioRegionAdapter; }
    
    int getNumAudioTrackChannels() const;
    
    bool anyChannelSolo() const;
    
    juce::Colour getNewAudioTrackColour() const;
    
    void copySelectedChannelsToNewTrack();
    
    std::shared_ptr<audium::AudioBusInterface> audioBusInterface;
    
    std::vector<std::shared_ptr<AudioTrack>> audioTracks;
    
    
    
private:
    std::shared_ptr<juce::UndoManager> undoManager;
    std::shared_ptr<TempoProvider> tempoProvider;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<TransportSourceContainer> transportSourceContainer;
    std::shared_ptr<audium::SelectionManager> selectionManager;
    std::shared_ptr<audium::TransportLoop> transportLoop;
    
    int selectedGroup = 0;
    
    float masterGain = 1.f;

    
    // Discuss: inject depenendency
    AudioRegionAdapter audioRegionAdapter;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrackContainer)
};
