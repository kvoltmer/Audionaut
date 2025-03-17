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

#include <iostream>

#include "AudioTrackListBoxModel.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/ActionMessages.h"
#include "Interface/Components/MiddlePanel/AudioTrackBaseComponent.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

int AudioTrackListBoxModel::getNumRows()
{
    return audiumEngine->getAudioTrackContainer()->getNumItems();
}

void AudioTrackListBoxModel::paintListBoxItem ( int rowNumber,
                        juce::Graphics& g,
                        int width, int height,
                        bool rowIsSelected)
{
}

juce::Component* AudioTrackListBoxModel::refreshComponentForRow (int rowNumber, bool isRowSelected,
                                                                     juce::Component* existingComponentToUpdate)
{
    auto audioTrack = audiumEngine->getAudioTrackContainer()->getAudioTrack(rowNumber);
    if (existingComponentToUpdate == nullptr)
    {
        if (audioTrack != nullptr)
        {
            if (arrangementMode)
            {
                return new AudioTrackComponent(audioTrack, audiumEngine, zoomHandler, regionSelector);
            }
            else
            {
                return new AudioTrackRegionEditComponent(audioTrack, audiumEngine, zoomHandler, regionSelector);
            }
        }
    }
    else
    {
        auto component = dynamic_cast<AudioTrackBaseComponent*>(existingComponentToUpdate);
        jassert(component);
    
        if (audioTrack != nullptr)
        {
            // update of audioTrack since row might have changed after delete
            component->refreshComponent(audioTrack);
        }
        return component;
    }
    
    
    return nullptr;
}

int AudioTrackListBoxModel::getRowHeight (int rowNumber) const
{
    auto track = audiumEngine->getAudioTrackContainer()->getAudioTrack(rowNumber);
    if (track != nullptr)
        return track->getTotalHeight() + DraggerControl::draggerHeight;
    
    jassertfalse;
    return 0;
}


void AudioTrackListBoxModel::deleteKeyPressed (int lastRowSelected)
{
    audiumEngine->getAudioTrackContainer()->deleteSelectedObjects();
}

void AudioTrackListBoxModel::backgroundClicked (const juce::MouseEvent&)
{
    owner->deselectAllRows();
    audiumEngine->getAudioTrackContainer()->getSelectionManager()->deselectAll();
    audiumEngine->getAudioTrackContainer()->sendActionMessage(updateAll);
}

void AudioTrackListBoxModel::listWasScrolled()
{
    audiumEngine->getAudioTrackContainer()->sendActionMessage(scrolledVertically);
}

void AudioTrackListBoxModel::selectedRowsChanged (int lastRowSelected)
{
    auto selectedRows = owner->getSelectedRows();
    audiumEngine->getAudioTrackContainer()->setSelectedRows(selectedRows);
    audiumEngine->getAudioTrackContainer()->sendActionMessage(updateAll);
}

int AudioTrackListBoxModel::getExtraSpaceAtBottom() const
{
    return AudiumLookAndFeel::extraSpaceAtBottom;
}
