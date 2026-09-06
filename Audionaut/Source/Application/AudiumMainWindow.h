//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/AutoEdit/AutoEdit.h"
#include "Interface/Components/MainComponent.h"

class NewRegionDialog;
class ExportAudioDialog;
class NewAudioTrackDialog;
class AssembleDialog;
class StemSeparationDialog;

//==============================================================================
/*
    This class implements the desktop window that contains an instance of
    our MainComponent class.
*/
class AudiumMainWindow    : public juce::DocumentWindow, public juce::ApplicationCommandTarget
{
public:
    AudiumMainWindow (juce::String name, std::shared_ptr<audium::AudiumEngine> audiumEngine);
    ~AudiumMainWindow() override;

    void closeButtonPressed() override;
    
    std::shared_ptr<audium::AudiumEngine> getEngine() const { return audiumEngine; }
    
    ApplicationCommandTarget* getNextCommandTarget() override;
    void getAllCommands (Array <CommandID>& commands) override;
    void getCommandInfo (CommandID commandID, ApplicationCommandInfo& result) override;
    bool perform (const InvocationInfo& info) override;
    
private:
    
    bool anythingSelected();
    bool canPaste();

    // Starts an auto edit as an in-arrangement preview - the previewed clip
    // shows the overlay control (see AutoEditOverlayControl) - or cancels the
    // pending one. Replaces the old Auto Edit dialog.
    void toggleAutoEditPreview();

    // Whether the Auto Edit command is available: always while an edit is
    // pending (the command is also the cancel), otherwise only when the
    // target clip would yield at least one segment at the entry measure
    // value - a clip too short for a single cut greys the command out.
    bool canToggleAutoEditPreview();

    // Asks for the sequence length (see AssembleDialog), then rebuilds the
    // target track's playlist as a song assembled from its regions in the
    // given mode (see AutoEdit::invokeAssemble). The mode comes from the
    // menu item picked in the Assemble sub menu.
    void invokeAssemble(audium::AssembleConfig::Mode mode);

    // Whether the Assemble commands are available: the target track needs at
    // least one region with audio to arrange.
    bool canAssemble();

    // Whether Separate Stems is available: a build with Demucs, exactly one
    // clip selected, transport stopped. The model itself is checked (and
    // offered for download) when the command runs.
    bool canSeparateStems();

    // Splits the selected clip into stem tracks - see StemSeparationDialog.
    void invokeSeparateStems();

    // Whether the Stretch Clip overlay can be toggled: an open overlay can
    // always be dismissed; otherwise a clip must be selected.
    bool canToggleStretchOverlay();

    // Opens the stretch overlay on the selected clip, or dismisses an open
    // one (which commits its pending edit as one undo step).
    void toggleStretchOverlay();

    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<MainComponent> mainComponent;

    std::unique_ptr<NewRegionDialog> newRegionDialog;
    std::unique_ptr<ExportAudioDialog> exportAudioDialog;
    std::unique_ptr<NewAudioTrackDialog> newAudioTrackDialog;
    std::unique_ptr<AssembleDialog> assembleDialog;
    std::unique_ptr<StemSeparationDialog> stemSeparationDialog;
        
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumMainWindow)
};
