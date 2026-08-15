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
    MiddlePanelComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine_) :
        audiumEngine(audiumEngine_)
    {
        auto playListScheduler = audiumEngine->getPlayListScheduler();
        snapToGridHandler.reset(new SnapToGridHandler(playListScheduler));
        zoomHandler.reset(new ZoomHandler(playListScheduler, snapToGridHandler));

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
            auto verticalScrollOffset = arrangementComponent->getVerticalScrollOffset();
            auto horizontalScrollOffset = arrangementComponent->getHorizontalScrollOffset();
            createComponents();
            
            arrangementComponent->updateUI();
            channelsComponent->updateUI();
            
            arrangementComponent->setVerticalScrollOffset(verticalScrollOffset);
            arrangementComponent->setHorizontalScrollOffset(horizontalScrollOffset);
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
    
    /**
     * Writes the arrangement's zoom factor and scroll position into the project's UI state.
     */
    void writeUiState(json& uiState)
    {
        json jsonArrangement;
        jsonArrangement["zoom_factor"] = zoomHandler->getZoomFactor();
        jsonArrangement["scroll_x"] = arrangementComponent->getHorizontalScrollOffset();
        jsonArrangement["scroll_y"] = arrangementComponent->getVerticalScrollOffset();
        uiState["arrangement"] = jsonArrangement;
    }

    /**
     * Restores the arrangement's zoom factor and scroll position from the project's UI state.
     * Missing entries (new or older projects) fall back to the defaults.
     */
    void readUiState(const json& uiState)
    {
        auto zoomFactor = ZoomHandler::defaultZoomFactor;
        auto scrollX = 0.0;
        auto scrollY = 0.0;

        if (uiState.contains("arrangement")) {
            const auto& jsonArrangement = uiState["arrangement"];

            if (jsonArrangement.contains("zoom_factor"))
                zoomFactor = jsonArrangement["zoom_factor"].template get<double>();

            if (jsonArrangement.contains("scroll_x"))
                scrollX = jsonArrangement["scroll_x"].template get<double>();

            if (jsonArrangement.contains("scroll_y"))
                scrollY = jsonArrangement["scroll_y"].template get<double>();
        }

        zoomHandler->setZoomFactor(zoomFactor);

        // the scroll bar ranges are only valid once the content has been laid
        // out for the restored zoom factor
        arrangementComponent->updateUI();

        arrangementComponent->setHorizontalScrollOffset(scrollX);
        arrangementComponent->setVerticalScrollOffset(scrollY);

        arrangementComponent->onScrollContext();
        channelsComponent->setVerticalScrollOffset(arrangementComponent->getVerticalScrollOffset());
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
