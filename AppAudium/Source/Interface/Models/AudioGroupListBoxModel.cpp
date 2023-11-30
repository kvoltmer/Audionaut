
#include <iostream>

#include "AudioGroupListBoxModel.h"
#include "Engine/AudioGroupContainer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/ActionMessages.h"
#include "Interface/Components/MiddlePanel/GroupBaseComponent.h"

int AudioGroupListBoxModel::getNumRows()
{
    return audiumEngine->getAudioGroupContainer()->getNumItems();
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
    auto audioGroup = audiumEngine->getAudioGroupContainer()->getAudioGroup(rowNumber);
    if (existingComponentToUpdate == nullptr)
    {
        if (audioGroup != nullptr)
        {
            if (arrangementMode)
            {
                return new ArrangementGroupComponent(audioGroup, audiumEngine, zoomHandler, regionSelector);
            }
            else
            {
                return new EditGroupComponent(audioGroup, audiumEngine, zoomHandler, regionSelector);
            }
        }
    }
    else
    {
        auto component = dynamic_cast<GroupBaseComponent*>(existingComponentToUpdate);
        jassert(component);
    
        if (audioGroup != nullptr)
        {
            // update of audioGroup since row might have changed after delete
            component->refreshComponent(audioGroup);
        }
        return component;
    }
    
    
    return nullptr;
}

int AudioGroupListBoxModel::getRowHeight (int rowNumber) const
{
    auto audioGroupContainer = audiumEngine->getAudioGroupContainer();
    auto audioResourceContainer = audiumEngine->getAudioResourceContainer();
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


void AudioGroupListBoxModel::deleteKeyPressed (int lastRowSelected)
{
    auto selected = owner->getSelectedRows();
    auto audioGroupContainer = audiumEngine->getAudioGroupContainer();
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
    audiumEngine->getAudioGroupContainer()->sendActionMessage(arrangementScrolled);
}
