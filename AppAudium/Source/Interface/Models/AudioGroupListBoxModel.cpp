
#include <iostream>

#include "AudioGroupListBoxModel.h"

AudioGroupListBoxModel::AudioGroupListBoxModel(std::shared_ptr<AudioGroupListBox> owner,
                                               std::shared_ptr<AudiumEngine> audiumEngine,
                                               std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                                               std::shared_ptr<PlayListContainer> playListContainer,
                                               std::shared_ptr<ZoomHandler> zoomHandler,
                                               std::shared_ptr<RegionSelector> regionSelector) :
    owner(owner),
    audiumEngine(audiumEngine),
    audioResourceContainer(audioResourceContainer),
    playListContainer(playListContainer),
    zoomHandler(zoomHandler),
    regionSelector(regionSelector)
{
}

AudioGroupListBoxModel::~AudioGroupListBoxModel()
{
}

int AudioGroupListBoxModel::getNumRows()
{
    return audioResourceContainer->getNumAudioResourceGroups();
}

void AudioGroupListBoxModel::paintListBoxItem ( int rowNumber,
                        juce::Graphics& g,
                        int width, int height,
                        bool rowIsSelected)
{
    if (rowIsSelected)
    {
        auto thumbArea = Rectangle<int>(0, 0, width, height);
        g.setColour (Colours::lightgrey);
        g.drawRoundedRectangle (thumbArea.toFloat(), 3.0f, 2.0f);
    }
}

juce::Component* AudioGroupListBoxModel::refreshComponentForRow (int rowNumber, bool isRowSelected,
                                                                     juce::Component* existingComponentToUpdate)
{
    if (existingComponentToUpdate == nullptr)
    {
        auto audioResourceGroup = audioResourceContainer->getAudioResourceGroup(rowNumber);
        if (audioResourceGroup != nullptr)
        {
            auto component = new AudioGroupComponent(audioResourceGroup, audiumEngine, zoomHandler);
            return component;
        }
    }
    else
    {
        auto component = dynamic_cast<AudioGroupComponent*>(existingComponentToUpdate);
        jassert(component);
        auto audioResourceGroup = audioResourceContainer->getAudioResourceGroup(rowNumber);
        if (audioResourceGroup != nullptr)
        {
            // update of audioResourceGroup since row might have changed after delete
            component->setAudioResourceGroup(audioResourceGroup);
        }
        return component;
    }
    
    
    return nullptr;
}

int AudioGroupListBoxModel::getRowHeight (int rowNumber) const
{
    if (rowNumber < audioResourceContainer->getNumAudioResourceGroups())
    {
        auto group = audioResourceContainer->getAudioResourceGroup(rowNumber);
        int height = 0;
        auto audioResources = audioResourceContainer->getAudioResourcesForGroup(group);
        for (auto audioResource : audioResources)
        {
            height += audioResource->getHeight();
        }
        return height;
        
    }
    jassertfalse;
    return 0;
}

void AudioGroupListBoxModel::selectedRowsChanged (int lastRowSelected)
{
    std::cout << "selectedRowsChanged: " << lastRowSelected << std::endl;
}

void AudioGroupListBoxModel::deleteKeyPressed (int lastRowSelected)
{
    auto selected = owner->getSelectedRows();
    
    for (int i = selected.size()-1; i >= 0; i--)
    {
        std::cout << "delete selected = " << selected[i] << std::endl;
        
        audioResourceContainer->removeAudioResourceGroup(selected[i]);
    }
    
    owner->updateContent();
}

void AudioGroupListBoxModel::listWasScrolled()
{
    jassert(regionSelector);
    regionSelector->updateFromEngine();
}

