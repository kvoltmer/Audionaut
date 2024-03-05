/*
  ==============================================================================

    MiddlePanelComponent.h
    Created: 6 Jun 2023 11:51:48am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"

#include "Interface/Components/MiddlePanel/ChannelView/ChannelsComponent.h"
#include "Interface/Components/MiddlePanel/ArrangementView/ArrangementComponent.h"
#include "Interface/Components/MiddlePanel/EditView/EditComponent.h"
#include "Interface/AudiumLookAndFeel.h"
#include "Interface/Handlers/ZoomHandler.h"

class AudiumEngine;

class MiddlePanelComponent : public juce::Component
{
public:
    MiddlePanelComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
        audiumEngine(audiumEngine)
    {
        zoomHandler.reset(new ZoomHandler(audiumEngine->getPlayListScheduler()));

        createComponents();
    }
    
    ~MiddlePanelComponent() override
    {
    }
    
    void createComponents()
    {
        const auto visibleRange = zoomHandler->getVisibleRange();
        
        removeAllChildren();
        
        channelsComponent.reset(new ChannelsComponent(audiumEngine));
        addAndMakeVisible(channelsComponent.get());
        
        arrangementComponent.reset(new ArrangementComponent(audiumEngine, zoomHandler));
        addAndMakeVisible(arrangementComponent.get());
        
        editComponent.reset(new EditComponent(audiumEngine, zoomHandler));
        addAndMakeVisible(editComponent.get());
        
        // TODO: restore state
        editComponent->setVisible(false);
        
        zoomHandler->setVisibleRange(visibleRange, sendNotificationSync);
    }
    
    enum UIContext {
        EntireContext,
        VerticalScrollContext,
        ForceRebuildContext,
        ArrangementContext
    };
    
        void paint (juce::Graphics& g) override
        {
            // fill background
            g.fillAll (findColour(audium::listBoxBackgroundColourId));
        }
    
    void resized() override
    {
        auto localBounds = getLocalBounds();
        auto channelBounds = localBounds.removeFromLeft(AudiumLookAndFeel::channelsWidth);
        channelBounds.removeFromTop(AudiumLookAndFeel::dragZoomControlHeight);
        channelsComponent->setBounds(channelBounds);
        
        arrangementComponent->setBounds(localBounds);
        editComponent->setBounds(localBounds);
    }
    
    void updateUI(UIContext context = EntireContext)
    {
        if (context == EntireContext)
        {
            arrangementComponent->updateUI();
            channelsComponent->updateUI();
            editComponent->updateUI();
        }
        else if(context == VerticalScrollContext)
        {
            getVisibleComponent()->onScrollContext();
            if (editComponent->isVisible())
            {
                channelsComponent->setVerticalScrollOffset(editComponent->getVerticalScrollOffset());
            }
            else if (arrangementComponent->isVisible())
            {
                channelsComponent->setVerticalScrollOffset(arrangementComponent->getVerticalScrollOffset());
            }
        }
        else if (context == ForceRebuildContext)
        {
            bool editMode = editComponentVisible();
            createComponents();
            
            arrangementComponent->updateUI();
            channelsComponent->updateUI();
            editComponent->updateUI();
            
            showEditComponent(editMode);
            resized();
        }
        else if (context == ArrangementContext)
        {
            arrangementComponent->updateUI();
            editComponent->updateUI();
        }
    }
    
    void zoomIn()
    {
        getVisibleComponent()->zoomIn();
    }
    
    void zoomOut()
    {
        getVisibleComponent()->zoomOut();
    }
    
    void pageLeft()
    {
        getVisibleComponent()->pageLeft();
    }
    
    void pageRight()
    {
        getVisibleComponent()->pageRight();
    }
    
    void showArrangementComponent(bool visible)
    {
        arrangementComponent->setVisible(visible);
        if (visible)
        {
            arrangementComponent->resized();
            arrangementComponent->updateUI();
            channelsComponent->setVerticalScrollOffset(arrangementComponent->getVerticalScrollOffset());
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
            channelsComponent->setVerticalScrollOffset(editComponent->getVerticalScrollOffset());
        }
    }
    
    bool editComponentVisible() const
    {
        return editComponent->isVisible();
    }
    
    ArrangementEditBaseComponent* getVisibleComponent() const
    {
        if (arrangementComponent->isVisible())
            return arrangementComponent.get();
        else
            return editComponent.get();
    }
    
private:
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    
    std::unique_ptr<ChannelsComponent> channelsComponent;
    std::unique_ptr<ArrangementComponent> arrangementComponent;
    std::unique_ptr<EditComponent> editComponent;
    
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiddlePanelComponent)
};
