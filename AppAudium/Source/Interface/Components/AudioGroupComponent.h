/*
  ==============================================================================

    AudioGroupComponent.h
    Created: 27 Nov 2022 3:25:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>

#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Controls/RegionSelector.h"

using namespace juce;

class AudioResourceGroup;
class PlayListContainer;
class AudioGroupRegionComponent;

//==============================================================================
/// TODO: discuss class name: maybe TrackComponent suites better
/*
 Display a AudioResourceGroup as part of AudioGroupListBoxModel.
 
 A AudioResourceGroup may contain multiple regions. The PlayListContainer holds the region information
 */
class AudioGroupComponent  : public Component,
                           public FileDragAndDropTarget,
                           public ChangeBroadcaster,
                           public ScrollBar::Listener
{
public:
    AudioGroupComponent (std::shared_ptr<AudioResourceGroup> audioResourceGroup,
                       std::shared_ptr<PlayListContainer> playListContainer,
                       std::shared_ptr<ZoomHandler> zoomHandler);

    ~AudioGroupComponent() override;
    
    void setAudioResourceGroup (std::shared_ptr<AudioResourceGroup> audioResourceGroup);

    void setFollowsTransport (bool shouldFollow);

    void resized() override;

    bool isInterestedInFileDrag (const StringArray& /*files*/) override;

    void filesDropped (const StringArray& files, int /*x*/, int /*y*/) override;

    void mouseDown (const MouseEvent& e) override;

    void mouseDrag (const MouseEvent& e) override;

    void mouseUp (const MouseEvent&) override;

    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel) override;
    
private:
    
    std::shared_ptr<AudioResourceGroup> audioResourceGroup;
    std::shared_ptr<PlayListContainer> playListContainer;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::vector<std::shared_ptr<AudioGroupRegionComponent>> audioGroupRegions;

    void scrollBarMoved (ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGroupComponent)
    
};
