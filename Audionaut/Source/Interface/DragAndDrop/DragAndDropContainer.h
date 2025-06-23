/*
  ==============================================================================

    DragAndDropContainer.h
    Created: 27 May 2025 6:07:53pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace audium {
    
class DragAndDropContainer : public juce::DragAndDropContainer {
    
    
    /** Override this if you want to be able to perform an external drag of a set of files
        when the user drags outside of this container component.

        This method will be called when a drag operation moves outside the JUCE window,
        and if you want it to then perform a file drag-and-drop, add the filenames you want
        to the array passed in, and return true.

        @param sourceDetails    information about the source of the drag operation
        @param files            on return, the filenames you want to drag
        @param canMoveFiles     on return, true if it's ok for the receiver to move the files; false if
                                it must make a copy of them (see the performExternalDragDropOfFiles() method)
        @see performExternalDragDropOfFiles, shouldDropTextWhenDraggedExternally
    */
// TODO: implement this method
//    bool shouldDropFilesWhenDraggedExternally (const DragAndDropTarget::SourceDetails& sourceDetails,
//                                                       StringArray& files, bool& canMoveFiles) override;
};
    

} // namespace audium

