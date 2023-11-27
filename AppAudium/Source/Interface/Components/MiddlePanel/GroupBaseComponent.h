/*
  ==============================================================================

    GroupBaseComponent.h
    Created: 27 Nov 2023 12:17:09pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Controls/RegionSelector.h"

class AudioGroup;
class PlayListContainer;
class PlayListItemComponent;

//==============================================================================
/*
*/
class GroupBaseComponent  : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    GroupBaseComponent (std::shared_ptr<AudioGroup> audioGroup,
                         std::shared_ptr<AudiumEngine> audiumEngine,
                         std::shared_ptr<ZoomHandler> zoomHandler);
    
    virtual ~GroupBaseComponent() {};
    
    virtual void refreshComponent (std::shared_ptr<AudioGroup> audioGroup, bool forceRebuildComponents = false) = 0;

    void paint (juce::Graphics&) override;
    

    // drag & drop:
    void filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY) override;
    bool isInterestedInFileDrag (const juce::StringArray& /*files*/) override { return true; }
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    
    void mouseDown (const juce::MouseEvent& e) override;

    void mouseDrag (const juce::MouseEvent& e) override;

    void mouseUp (const juce::MouseEvent&) override;

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override;
    
protected:
    
    std::shared_ptr<AudioGroup> audioGroup;
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    
    bool externalDragAndDrop = false;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GroupBaseComponent)
};
