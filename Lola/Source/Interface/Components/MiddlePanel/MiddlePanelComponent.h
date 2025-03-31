//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"

#include "Interface/Components/MiddlePanel/ChannelView/ChannelsComponent.h"
#include "Interface/Components/MiddlePanel/ArrangementView/ArrangementComponent.h"
#include "Interface/Components/MiddlePanel/EditView/EditComponent.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Handlers/SnapToGridHandler.h"

class MiddlePanelComponent : public juce::Component
{
public:
    MiddlePanelComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine) :
        audiumEngine(audiumEngine)
    {
        snapToGridHandlers[arrangementType].reset(new SnapToGridHandler());
        zoomHandler[arrangementType].reset(new ZoomHandler(audiumEngine->getPlayListScheduler(), snapToGridHandlers[arrangementType]));
        
        snapToGridHandlers[editType].reset(new SnapToGridHandler());
        zoomHandler[editType].reset(new ZoomHandler(audiumEngine->getPlayListScheduler(), snapToGridHandlers[editType]));

        createComponents();
    }
    
    ~MiddlePanelComponent() override
    {
    }
    
    void createComponents()
    {
        auto editMode = audiumEngine->getPlayListScheduler()->isEditMode();
        auto zoom = editMode ? zoomHandler[editType] : zoomHandler[arrangementType];
        const auto visibleRange = zoom->getVisibleRange();
        
        removeAllChildren();
        
        channelsComponent.reset(new ChannelsComponent(audiumEngine));
        addAndMakeVisible(channelsComponent.get());
        
        arrangementComponent.reset(new ArrangementComponent(audiumEngine, zoomHandler[arrangementType]));
        addAndMakeVisible(arrangementComponent.get());
        
        editComponent.reset(new EditComponent(audiumEngine, zoomHandler[editType]));
        addAndMakeVisible(editComponent.get());
        
        editComponent->setVisible(editMode);
        arrangementComponent->setVisible(!editMode);
        
        zoom->setVisibleRange(visibleRange, sendNotificationSync);
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
            bool editMode = audiumEngine->getPlayListScheduler()->isEditMode();
            auto scrollOffset = arrangementComponent->getVerticalScrollOffset();
            createComponents();
            
            arrangementComponent->updateUI();
            channelsComponent->updateUI();
            editComponent->updateUI();
            
            arrangementComponent->setVerticalScrollOffset(scrollOffset);
            showEditComponent(editMode);
            showArrangementComponent(!editMode);
            resized();
        }
        else if (context == ArrangementContext)
        {
            if (arrangementComponent->isVisible()) {
                arrangementComponent->updateUI();
            }
            else if (editComponent->isVisible()) {
                editComponent->updateUI();
            }
            else {
                jassertfalse;
            }
            
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
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    
    enum { arrangementType = 0, editType = 1, numTypes = 2 };
    
    std::shared_ptr<ZoomHandler> zoomHandler[numTypes];
    std::shared_ptr<SnapToGridHandler> snapToGridHandlers[numTypes];
    
    std::unique_ptr<ChannelsComponent> channelsComponent;
    std::unique_ptr<ArrangementComponent> arrangementComponent;
    std::unique_ptr<EditComponent> editComponent;
    
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiddlePanelComponent)
};
