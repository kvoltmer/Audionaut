
#pragma once

#include <JuceHeader.h>
#include "Interface/Widgets/audium_ListBox.h"

class AudiumEngine;
class ZoomHandler;
class AudioTrack;

//==============================================================================
/*
*/
class AudioTrackListBox  : public audium::ListBox, public juce::FileDragAndDropTarget
{
public:
    AudioTrackListBox (std::shared_ptr<AudiumEngine> audiumEngine,
                       std::shared_ptr<ZoomHandler> zoomHandler);
    ~AudioTrackListBox() override;
    
    // drag & drop
    void filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY) override;
    bool isInterestedInFileDrag (const juce::StringArray& /*files*/) override { return true; }
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    
    void setNewGroupColour(std::shared_ptr<AudioTrack> track);
    
    void resized() override;
    
private:
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    std::shared_ptr<AudioTrack> audioTrack;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrackListBox)
};
