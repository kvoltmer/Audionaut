/*
  ==============================================================================

    MiddlePanelComponent.h
    Created: 6 Jun 2023 11:51:48am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MiddlePanel/ChannelView/ChannelsComponent.h"
#include "Interface/Components/MiddlePanel/ArrangementView/ArrangementComponent.h"
#include "Interface/Components/MiddlePanel/EditView/EditComponent.h"

class AudiumEngine;

class MiddlePanelComponent : public juce::Component
{
public:
    MiddlePanelComponent(std::shared_ptr<AudiumEngine> audiumEngine)
    {
        channelsComponent.reset(new ChannelsComponent(audiumEngine));
        addAndMakeVisible(channelsComponent.get());
        
        arrangementComponent.reset(new ArrangementComponent(audiumEngine));
        addAndMakeVisible(arrangementComponent.get());
        
        editComponent.reset(new EditComponent(audiumEngine));
        addAndMakeVisible(editComponent.get());
        editComponent->setVisible(false);
    }
    
    ~MiddlePanelComponent() override
    {
    }
    
    enum UIContext {
        EntireContext,
        ArrangementScrollContext
    };
    
    void resized() override
    {
        auto channelsWidth = 60;
        
        channelsComponent->setBounds(getLocalBounds().removeFromLeft(channelsWidth));
        
        auto bounds = getLocalBounds().removeFromRight(getWidth() - channelsWidth);
        
        arrangementComponent->setBounds(bounds);
        editComponent->setBounds(bounds);
    }
    
    void updateUI(UIContext context = EntireContext)
    {
        if (context == EntireContext)
        {
            arrangementComponent->updateUI();
            channelsComponent->updateUI();
            editComponent->updateUI();
        }
        else if(context == ArrangementScrollContext)
        {
            arrangementComponent->getRegionSelector()->updateFromEngine();
        }
    }
    
    void zoomIn()
    {
        if (arrangementComponent->isVisible())
            arrangementComponent->zoomIn();
        
        if (editComponent->isVisible())
            editComponent->zoomIn();
    }
    
    void zoomOut()
    {
        if (arrangementComponent->isVisible())
            arrangementComponent->zoomOut();
        
        if (editComponent->isVisible())
            editComponent->zoomOut();
    }
    
    void showArrangementComponent(bool visible)
    {
        arrangementComponent->setVisible(visible);
        if (visible)
        {
            arrangementComponent->resized();
            arrangementComponent->updateUI();
        }
    }
    
    bool arrangementComponentVisible() const
    {
        return arrangementComponent->isVisible();
    }
    
    
    void showEditComponent(bool visible)
    {
        editComponent->setVisible(visible);
        if (visible)
        {
            editComponent->resized();
            editComponent->updateUI();
        }
    }
    
    bool editComponentVisible() const
    {
        return editComponent->isVisible();
    }
    
private:
    
    std::unique_ptr<ChannelsComponent> channelsComponent;
    std::unique_ptr<ArrangementComponent> arrangementComponent;
    std::unique_ptr<EditComponent> editComponent;
    
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiddlePanelComponent)
};
