/*
  ==============================================================================

    WaveFormComponent.h
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


class AudioResource;


//==============================================================================
class WaveFormComponent  : public Component,
                           public ChangeListener,
                           public FileDragAndDropTarget,
                           public ChangeBroadcaster,
                           public ScrollBar::Listener
{
public:
    WaveFormComponent (std::shared_ptr<AudioResource> audioResource,
                       std::shared_ptr<ZoomHandler> zoomHandler);

    ~WaveFormComponent() override;
    
    void setAudioResource (std::shared_ptr<AudioResource> audioResource);

    void setFollowsTransport (bool shouldFollow);

    void paint (Graphics& g) override;

    void resized() override;

    void changeListenerCallback (ChangeBroadcaster*) override;

    bool isInterestedInFileDrag (const StringArray& /*files*/) override;

    void filesDropped (const StringArray& files, int /*x*/, int /*y*/) override;

    void mouseDown (const MouseEvent& e) override;

    void mouseDrag (const MouseEvent& e) override;

    void mouseUp (const MouseEvent&) override;

    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel) override;
    
private:
    
    std::shared_ptr<AudioResource> audioResource;

    std::shared_ptr<ZoomHandler> zoomHandler;
    
    std::unique_ptr<ResizableEdgeComponent> resizableEdgeComponent;
    std::unique_ptr<ResizableBorderComponent> resizableBorderComponent;

    void scrollBarMoved (ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveFormComponent)
    
};
