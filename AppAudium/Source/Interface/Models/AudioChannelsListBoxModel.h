/*
  ==============================================================================

    AudioChannelsListBoxModel.h
    Created: 23 Oct 2023 3:14:04pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Widgets/audium_ListBox.h"
#include "Interface/Components/MiddlePanel/ChannelComponent.h"
#include "Engine/AudiumEngine.h"

class AudioChannelsListBoxModel  : public audium::ListBoxModel
{
public:
    AudioChannelsListBoxModel(std::shared_ptr<audium::ListBox> owner,
                              std::shared_ptr<AudiumEngine> audiumEngine) :
        owner(owner),
        audiumEngine(audiumEngine)
    {
    }
    
    ~AudioChannelsListBoxModel() override
    {
    }
    
    int getNumRows() override
    {
        return audiumEngine->getAudioResourceContainer()->getNumChannels();
    }

    void paintListBoxItem ( int rowNumber,
                            juce::Graphics& g,
                            int width, int height,
                            bool rowIsSelected) override
    {
    }
    
    juce::Component* refreshComponentForRow (   int rowNumber, bool isRowSelected,
                                                juce::Component* existingComponentToUpdate) override
    {
        auto audioResource = audiumEngine->getAudioResourceContainer()->getChannel(rowNumber);
        if (existingComponentToUpdate == nullptr)
        {
            if (audioResource != nullptr)
            {
                auto component = new ChannelComponent(audioResource, audiumEngine);
                return component;
            }
        }
        else
        {
            auto component = dynamic_cast<ChannelComponent*>(existingComponentToUpdate);
            jassert(component);
        
            if (audioResource != nullptr)
            {
                // update of audioGroup since row might have changed after delete
                component->refreshComponent(audioResource, isRowSelected);
            }
            return component;
        }
        
        
        return nullptr;
    }

    
    int getRowHeight (int rowNumber) const override
    {
        auto audioResource = audiumEngine->getAudioResourceContainer()->getChannel(rowNumber);
        if (audioResource != nullptr)
            return audioResource->getChannelHeight();

        jassertfalse;
        return 0;
    }
    
    void deleteKeyPressed (int lastRowSelected) override
    {
        auto audioResource = audiumEngine->getAudioResourceContainer()->getChannel(lastRowSelected);
        if (audioResource != nullptr)
        {
            auto component = dynamic_cast<ChannelComponent*>(owner->getComponentForRowNumber(lastRowSelected));
            if (component)
                component->stopTheTimer();
            
            audiumEngine->getAudioResourceContainer()->removeAudioResource(audiumEngine, audioResource);
        }
    }
    
    void backgroundClicked (const juce::MouseEvent&) override
    {
        owner->deselectAllRows();
    }
    
    void listWasScrolled() override {}
    
    void selectedRowsChanged (int lastRowSelected) override
    {
    }
        
private:
    std::shared_ptr<audium::ListBox> owner;
    std::shared_ptr<AudiumEngine> audiumEngine;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioChannelsListBoxModel)
};
