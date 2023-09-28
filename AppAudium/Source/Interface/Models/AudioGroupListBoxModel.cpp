
#include <iostream>

#include "AudioGroupListBoxModel.h"

AudioGroupListBoxModel::AudioGroupListBoxModel(std::shared_ptr<AudioGroupListBox> owner,
                                                     std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                                                     std::shared_ptr<PlayListContainer> playListContainer,
                                                     std::shared_ptr<ZoomHandler> zoomHandler,
                                                     std::shared_ptr<RegionSelector> regionSelector) :
    audioResourceContainer(audioResourceContainer),
    playListContainer(playListContainer),
    zoomHandler(zoomHandler),
    regionSelector(regionSelector),
    owner(owner)
{
}

AudioGroupListBoxModel::~AudioGroupListBoxModel()
{
}

int AudioGroupListBoxModel::getNumRows()
{
    return audioResourceContainer->getNumAudioResources();
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
        auto audioResource = audioResourceContainer->getAudioResource(rowNumber);
        if (audioResource != nullptr)
        {
            auto component = new WaveFormComponent(audioResource, playListContainer, zoomHandler);
            return component;
        }
    }
    else
    {
        auto component = dynamic_cast<WaveFormComponent*>(existingComponentToUpdate);
        jassert(component);
        auto audioResource = audioResourceContainer->getAudioResource(rowNumber);
        if (audioResource != nullptr)
        {
            // update of audioResource since row might have changed after delete
            component->setAudioResource(audioResource);
        }
        return component;
    }
    
    
    return nullptr;
}

int AudioGroupListBoxModel::getRowHeight (int rowNumber) const
{
    if (rowNumber < audioResourceContainer->getNumAudioResources())
    {
        auto audioResource = audioResourceContainer->getAudioResource(rowNumber);
        return audioResource->getHeight();
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
//    if (AudioGroupListBox* list = this->findParentComponentOfClass<AudioGroupListBox>())
//    {
//        list->updateContent();
//    }
    
    //std::cout << "deleteKeyPressed: " << lastRowSelected << std::endl;
    auto selected = owner->getSelectedRows();
    
    for (int i = selected.size()-1; i >= 0; i--)
    {
        std::cout << "selected = " << selected[i] << std::endl;
        
        audioResourceContainer->removeAudioResource(selected[i]);
        
    }
    std::cout << "---------" << std::endl;
    
    owner->updateContent();
}

void AudioGroupListBoxModel::listWasScrolled()
{
    jassert(regionSelector);
    regionSelector->updateFromEngine();
}

