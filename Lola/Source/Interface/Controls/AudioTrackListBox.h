
#pragma once

#include <JuceHeader.h>
#include "Interface/Widgets/audium_ListBox.h"

class AudiumEngine;
class ZoomHandler;
class AudioTrack;


class AudioTrackListBox  : public audium::ListBox, public juce::FileDragAndDropTarget, public juce::DragAndDropTarget
{
public:
    AudioTrackListBox (std::shared_ptr<AudiumEngine> audiumEngine,
                       std::shared_ptr<ZoomHandler> zoomHandler);
    ~AudioTrackListBox() override;
    
    /// FileDragAndDropTarget overrides
    void filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY) override;
    bool isInterestedInFileDrag (const juce::StringArray& /*files*/) override { return true; }
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    
    /// DragAndDropTarget overrides
    bool isInterestedInDragSource (const SourceDetails &dragSourceDetails) override;
    void itemDragEnter (const SourceDetails &dragSourceDetails) override;
    void itemDragMove (const SourceDetails &dragSourceDetails) override;
    void itemDragExit (const SourceDetails &dragSourceDetails) override;
    void itemDropped (const SourceDetails &dragSourceDetails) override;
    bool shouldDrawDragImageWhenOver () override { return true; }
    
    void setNewGroupColour(std::shared_ptr<AudioTrack> track);
        
    void paint (juce::Graphics& g) override;
    
private:
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<ZoomHandler> zoomHandler;
    
    bool externalDragAndDrop = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrackListBox)
};
