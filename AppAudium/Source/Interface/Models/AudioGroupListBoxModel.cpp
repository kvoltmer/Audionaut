
#include <iostream>

#include "AudioGroupListBoxModel.h"
#include "Engine/AudioGroupContainer.h"
#include "Engine/AudiumEngine.h"

AudioGroupListBoxModel::AudioGroupListBoxModel(std::shared_ptr<AudioGroupListBox> owner,
                                               std::shared_ptr<AudiumEngine> audiumEngine,
                                               std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                                               std::shared_ptr<AudioGroupContainer> audioGroupContainer,
                                               std::shared_ptr<ZoomHandler> zoomHandler,
                                               std::shared_ptr<RegionSelector> regionSelector) :
    owner(owner),
    audiumEngine(audiumEngine),
    audioResourceContainer(audioResourceContainer),
    audioGroupContainer(audioGroupContainer),
    zoomHandler(zoomHandler),
    regionSelector(regionSelector)
{
}

AudioGroupListBoxModel::~AudioGroupListBoxModel()
{
}

int AudioGroupListBoxModel::getNumRows()
{
    return audioGroupContainer->getNumItems();
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
    auto audioGroup = audioGroupContainer->getAudioGroup(rowNumber);
    if (existingComponentToUpdate == nullptr)
    {
        if (audioGroup != nullptr)
        {
            auto component = new AudioGroupComponent(audioGroup, audiumEngine, zoomHandler);
            return component;
        }
    }
    else
    {
        auto component = dynamic_cast<AudioGroupComponent*>(existingComponentToUpdate);
        jassert(component);
    
        if (audioGroup != nullptr)
        {
            // update of audioGroup since row might have changed after delete
            component->setAudioGroup(audioGroup);
        }
        return component;
    }
    
    
    return nullptr;
}

int AudioGroupListBoxModel::getRowHeight (int rowNumber) const
{
    if (rowNumber < audioGroupContainer->getNumItems())
    {
        auto group = audioGroupContainer->getAudioGroup(rowNumber);
        int height = 0;
        auto audioResources = audioResourceContainer->getAudioResourcesForGroup(group.get());
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
        auto group = audioGroupContainer->getAudioGroup(selected[i]);
        if (group != nullptr)
        {
            
            audioGroupContainer->removeAudioGroup(audiumEngine, group);
        }
        else
        {
            jassertfalse;
        }
    }    
}

void AudioGroupListBoxModel::listWasScrolled()
{
    jassert(regionSelector);
    regionSelector->updateFromEngine();
}

