//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"

#include "Interface/Components/MiddlePanel/ChannelView/ChannelsComponent.h"
#include "Interface/Components/MiddlePanel/ArrangementView/ArrangementComponent.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Handlers/SnapToGridHandler.h"

class MiddlePanelComponent : public juce::Component
{
public:
    MiddlePanelComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine) :
        audiumEngine(audiumEngine)
    {
        snapToGridHandler.reset(new SnapToGridHandler());
        zoomHandler.reset(new ZoomHandler(audiumEngine->getPlayListScheduler(), snapToGridHandler));

        createComponents();
    }
    
    ~MiddlePanelComponent() override
    {
    }
    
    void createComponents()
    {
        auto editMode = audiumEngine->getPlayListScheduler()->isEditMode();
        
        if (editMode) {
            juce::NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                        "The region edit view was discontinued.",
                                                        "We will now display the arrangement view.");
            audiumEngine->getPlayListScheduler()->setEditMode(false);
        }

        removeAllChildren();
        
        channelsComponent.reset(new ChannelsComponent(audiumEngine));
        addAndMakeVisible(channelsComponent.get());
        
        arrangementComponent.reset(new ArrangementComponent(audiumEngine, zoomHandler));
        addAndMakeVisible(arrangementComponent.get());

        const auto visibleRange = zoomHandler->getVisibleRange();
    
        arrangementComponent->setVisible(true);
        
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
    }
    
    void updateUI(UIContext context = EntireContext)
    {
        if (context == EntireContext) {
            arrangementComponent->updateUI();
            channelsComponent->updateUI();
        }
        else if(context == VerticalScrollContext) {
            if (arrangementComponent->isVisible()) {
                arrangementComponent->onScrollContext();
                channelsComponent->setVerticalScrollOffset(arrangementComponent->getVerticalScrollOffset());
            }
        }
        else if (context == ForceRebuildContext) {
            bool editMode = audiumEngine->getPlayListScheduler()->isEditMode();
            auto scrollOffset = arrangementComponent->getVerticalScrollOffset();
            createComponents();
            
            arrangementComponent->updateUI();
            channelsComponent->updateUI();
            
            arrangementComponent->setVerticalScrollOffset(scrollOffset);
            showArrangementComponent(!editMode);
            resized();
        }
        else if (context == ArrangementContext) {
            if (arrangementComponent->isVisible()) {
                arrangementComponent->updateUI();
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
    
    bool editComponentVisible() const
    {
        return false;
    }
    
    ArrangementEditBaseComponent* getVisibleComponent() const
    {
        if (arrangementComponent->isVisible())
            return arrangementComponent.get();
        else {
            jassertfalse;
            return nullptr;
        }
    }
    
private:
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<SnapToGridHandler> snapToGridHandler;
    
    std::unique_ptr<ChannelsComponent> channelsComponent;
    std::unique_ptr<ArrangementComponent> arrangementComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiddlePanelComponent)
};
