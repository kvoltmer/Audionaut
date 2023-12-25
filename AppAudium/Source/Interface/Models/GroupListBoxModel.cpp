
#include <iostream>

#include "GroupListBoxModel.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/ActionMessages.h"
#include "Interface/Components/MiddlePanel/GroupBaseComponent.h"

int GroupListBoxModel::getNumRows()
{
    return audiumEngine->getAudioGroupContainer()->getNumItems();
}

void GroupListBoxModel::paintListBoxItem ( int rowNumber,
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

juce::Component* GroupListBoxModel::refreshComponentForRow (int rowNumber, bool isRowSelected,
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

int GroupListBoxModel::getRowHeight (int rowNumber) const
{
    auto group = audiumEngine->getAudioGroupContainer()->getAudioGroup(rowNumber);
    if (group != nullptr)
        return group->getTotalHeight() + DraggerControl::draggerHeight;
    
    jassertfalse;
    return 0;
}


void GroupListBoxModel::deleteKeyPressed (int lastRowSelected)
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

void GroupListBoxModel::listWasScrolled()
{
    audiumEngine->getAudioGroupContainer()->sendActionMessage(scrolledVertically);
}
