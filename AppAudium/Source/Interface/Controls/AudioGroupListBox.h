
#pragma once

#include <JuceHeader.h>
#include "Interface/Widgets/audium_ListBox.h"

class AudiumEngine;
class ZoomHandler;
class AudioGroup;

//==============================================================================
/*
*/
class AudioGroupListBox  : public audium::ListBox, public juce::FileDragAndDropTarget
{
public:
    AudioGroupListBox (std::shared_ptr<AudiumEngine> audiumEngine,
                       std::shared_ptr<ZoomHandler> zoomHandler);
    ~AudioGroupListBox() override;
    
    // drag & drop
    void filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY) override;
    bool isInterestedInFileDrag (const juce::StringArray& /*files*/) override { return true; }
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    
    void setNewGroupColour(std::shared_ptr<AudioGroup> group);
    
private:
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGroupListBox)
};
