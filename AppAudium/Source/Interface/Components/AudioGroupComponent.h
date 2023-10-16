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

class AudioGroup;
class PlayListContainer;
class AudioGroupRegionComponent;

//==============================================================================
/// TODO: discuss class name: maybe TrackComponent suites better
/*
 
 Display Regions in Timeline
 
 Display a AudioGroup as part of AudioGroupListBoxModel.
 
 A AudioGroup may contain multiple regions in the Timeline. The PlayListContainer holds the region information
 */
class AudioGroupComponent  : public Component,
                           public FileDragAndDropTarget
{
public:
    AudioGroupComponent (std::shared_ptr<AudioGroup> audioGroup,
                         std::shared_ptr<AudiumEngine> audiumEngine,
                         std::shared_ptr<ZoomHandler> zoomHandler);

    ~AudioGroupComponent() override;
    
    void refreshComponent (std::shared_ptr<AudioGroup> audioGroup, bool forceRebuildComponents = false);

    void paint (juce::Graphics&) override;
    
    void resized() override;

    // drag & drop:
    void filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY) override;
    bool isInterestedInFileDrag (const juce::StringArray& /*files*/) override { return true; }
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    
    void mouseDown (const MouseEvent& e) override;

    void mouseDrag (const MouseEvent& e) override;

    void mouseUp (const MouseEvent&) override;

    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel) override;
    
private:
    
    bool mustRebuildComponents() const;
    void rebuildComponents();
    
    std::shared_ptr<AudioGroup> audioGroup;
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    
    std::vector<std::shared_ptr<AudioGroupRegionComponent>> audioGroupRegions;
    
    bool externalDragAndDrop = false;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGroupComponent)
    
};
