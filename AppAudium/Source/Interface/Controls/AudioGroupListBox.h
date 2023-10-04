
#pragma once

#include <JuceHeader.h>
#include "Interface/Widgets/audium_ListBox.h"

class AudiumEngine;

//==============================================================================
/*
*/
class AudioGroupListBox  : public audium::ListBox, public juce::FileDragAndDropTarget
{
public:
    AudioGroupListBox (std::shared_ptr<AudiumEngine> audiumEngine,
                       const juce::String& componentName = juce::String(),
                       audium::ListBoxModel* model = nullptr);
    ~AudioGroupListBox() override;
    
    // drag & drop
    void filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY) override;
    bool isInterestedInFileDrag (const juce::StringArray& /*files*/) override { return true; }
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    
    
private:
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioGroupListBox)
};
