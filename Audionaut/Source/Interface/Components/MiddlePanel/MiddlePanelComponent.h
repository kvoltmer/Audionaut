//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"

#include "Interface/Components/MiddlePanel/ChannelView/ChannelsComponent.h"
#include "Interface/Components/MiddlePanel/ArrangementView/ArrangementComponent.h"
#include "Interface/Components/MixerPanel/MixerComponent.h"
#include "Interface/Controls/CollapsiblePanel.h"
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

        // the mixer's body height is its content's (set by setFixedSize below); its
        // open state is the user's and goes to the owner for persisting
        mixer.maximumSize = [this] { return mixerComponent != nullptr ? mixerComponent->getBodyHeight() : 0; };
        mixer.onRelayout = [this] { resized(); };
        mixer.onUserChange = [this] { NullCheckedInvocation::invoke (onMixerLayoutChanged); };

        createComponents();
    }

    ~MiddlePanelComponent() override
    {
    }

    void createComponents()
    {
        removeAllChildren();

        channelsComponent.reset(new ChannelsComponent(audiumEngine));
        addAndMakeVisible(channelsComponent.get());

        arrangementComponent.reset(new ArrangementComponent(audiumEngine, zoomHandler));
        addAndMakeVisible(arrangementComponent.get());

        mixerComponent.reset (new MixerComponent (audiumEngine));
        mixerComponent->onPreferredHeightChanged = [this] { mixer.setFixedSize (mixerComponent->getBodyHeight()); };
        mixer.setFixedSize (mixerComponent->getBodyHeight());

        // the mixer's header bar stays at the bottom and is its handle: pushed well down it
        // collapses the mixer until pulled back or released, pulled up it opens it; the mixer
        // cannot be made taller or shorter
        auto& header = mixerComponent->getHeader();
        header.onDragStart = [this] { mixer.dragStarted(); };
        header.onDrag = [this] (int distance) { mixer.dragged (distance); };
        header.onDragEnd = [this] { mixer.dragEnded(); };
        header.onDoubleClick = [this] { mixer.resetToDefault(); };
        header.onToggleClick = [this] { mixer.setOpen (! mixer.isOpen()); };
        addAndMakeVisible (mixerComponent.get());

        const auto visibleRange = zoomHandler->getVisibleRange();

        arrangementComponent->setVisible(true);

        zoomHandler->setVisibleRange(visibleRange, sendNotificationSync);
    }

    //==============================================================================
    // The mixer at the bottom. Whether it is open is an app preference, kept by
    // the owner through onMixerLayoutChanged; its height is its content's.

    /// Fires after the user opened or closed the mixer (by its seam, a button or a command).
    std::function<void()> onMixerLayoutChanged;

    bool isMixerVisible() const noexcept { return mixer.isOpen(); }
    void setMixerVisible (bool visible) { mixer.setOpen (visible); }

    /// From the preferences at start-up: no animation.
    void restoreMixerLayout (bool visible) { mixer.restore (visible, mixerComponent->getBodyHeight()); }

    /// The mixer's aux bus strips (an app preference the mixer keeps; View > Aux Buses).
    bool areAuxBusesVisible() const noexcept { return mixerComponent->areAuxBusesVisible(); }
    void setAuxBusesVisible (bool visible) { mixerComponent->setAuxBusesVisible (visible); }
    
    enum UIContext {
        EntireContext,
        VerticalScrollContext,
        ForceRebuildContext,
        ArrangementContext,
        ChannelsContext
    };
    
        void paint (juce::Graphics& g) override
        {
            // fill background
            g.fillAll (findColour(audium::listBoxBackgroundColourId));
        }
    
    void resized() override
    {
        auto localBounds = getLocalBounds();

        // the mixer's header bar always; under it as much of the body as is open
        const auto bodyHeight = mixer.getShownSize();
        if (bodyHeight > 0 && ! mixerComponent->isBodyShown())
            mixerComponent->updateUI();   // coming back: what it shows may be stale
        mixerComponent->setBounds (localBounds.removeFromBottom (MixerLayout::headerHeight + bodyHeight));
        mixerComponent->getHeader().setOpen (mixer.isOpen());

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
            mixerComponent->updateUI();
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
            mixerComponent->updateUI();
            
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
        else if (context == ChannelsContext) {
            channelsComponent->updateUI();
            mixerComponent->updateUI();
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
    std::unique_ptr<MixerComponent> mixerComponent;
    CollapsiblePanel mixer { 1, 1 };   // sized by setFixedSize from the mixer's content
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiddlePanelComponent)
};
