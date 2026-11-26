//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.


#include <JuceHeader.h>
#include "RightPanelComponent.h"
#include "Engine/AudiumEngine.h"
#include "Interface/ColourIds.h"
#include "Interface/Components/RightPanel/PlayListComponent.h"
#include "Interface/Components/RightPanel/PlayListContainerComponent.h"
#include "Interface/Components/RightPanel/RegionComponent.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#ifndef AUDIONAUT_HEADLESS
 #include "Application/AudiumApplication.h"
 #include "Util/Preferences.h"
#endif

RightPanelComponent::RightPanelComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine) :
    audiumEngine(audiumEngine)
{
    playListContainerComponent.reset(new PlayListContainerComponent(audiumEngine));
    regionComponent.reset(new RegionComponent(audiumEngine));

    // the region list leaves the playlists their minimum height; its open state and
    // height are the user's and are persisted
    regionList.maximumSize = [this] { return getHeight() - minPlayListsHeight - AudiumLookAndFeel::dividerHeight; };
    regionList.onRelayout = [this] { resized(); };
    regionList.onUserChange = [this] { writeLayoutPreferences(); };

    // the region list's header bar stays at the bottom and is its handle, as the mixer's
    // header bar is the mixer's: dragged up it makes the list taller, pushed well past the
    // minimum it collapses the list until pulled back or released, pulled up while closed it opens it
    regionListHeader.onDragStart = [this] { regionList.dragStarted(); };
    regionListHeader.onDrag = [this] (int distance) { regionList.dragged (distance); };
    regionListHeader.onDragEnd = [this] { regionList.dragEnded(); };
    regionListHeader.onDoubleClick = [this] { regionList.resetToDefault(); };
    regionListHeader.onToggleClick = [this] { regionList.setOpen (! regionList.isOpen()); };

    readLayoutPreferences();

    addAndMakeVisible(playListContainerComponent.get());
    addAndMakeVisible(regionListHeader);
    addAndMakeVisible(regionComponent.get());

    resized();
}

RightPanelComponent::~RightPanelComponent()
{

}

void RightPanelComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void RightPanelComponent::resized()
{
    auto bounds = getLocalBounds();

    // the region list's header bar always; under it as much of the list as is open
    const auto regionListHeight = regionList.getShownSize();
    regionComponent->setVisible (regionListHeight > 0);
    if (regionListHeight > 0)
        regionComponent->setBounds (bounds.removeFromBottom (regionListHeight));
    regionListHeader.setBounds (bounds.removeFromBottom (AudiumLookAndFeel::dividerHeight));
    regionListHeader.setOpen (regionList.isOpen());

    playListContainerComponent->setBounds (bounds);
}

void RightPanelComponent::updateUI(UIContext context)
{
    playListContainerComponent->updateUI(context);
    regionComponent->updateUI(context);

    resized();
}

void RightPanelComponent::readLayoutPreferences()
{
    auto visible = true;
    auto height = defaultRegionListHeight;
   #ifndef AUDIONAUT_HEADLESS   // the headless builds (tests, CLI) have no app preferences
    auto& preferences = AudiumApplication::getPreferences();
    visible = preferences.getValue (audium::PreferenceKeys::regionListVisible, "true") == "true";
    height = juce::String (preferences.getValue (audium::PreferenceKeys::regionListHeight, std::to_string (defaultRegionListHeight))).getIntValue();
   #endif
    regionList.restore (visible, height);
}

void RightPanelComponent::writeLayoutPreferences()
{
   #ifndef AUDIONAUT_HEADLESS
    auto& preferences = AudiumApplication::getPreferences();
    preferences.setValue (audium::PreferenceKeys::regionListVisible, regionList.isOpen() ? "true" : "false");
    preferences.setValue (audium::PreferenceKeys::regionListHeight, std::to_string (regionList.getSize()));
   #endif
}
