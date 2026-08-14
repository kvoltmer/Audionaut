//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudiumMainWindow.h"
#include "AudiumApplication.h"
#include "AudiumCommandIDs.h"
#include "Util/EngineAccess.h"
#include "Util/Preferences.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Region/AudioRegion.h"
#include "Interface/Dialogs/NewRegionDialog.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/AutoEdit/AutoEdit.h"
#include "Interface/Controls/AutoEditOverlayControl.h"
#include "Interface/Dialogs/ExportAudioDialog.h"
#include "Interface/Dialogs/NewAudioTrackDialog.h"
#include "Interface/Dialogs/AssembleDialog.h"

using namespace audium;

AudiumMainWindow::AudiumMainWindow (juce::String name, std::shared_ptr<audium::AudiumEngine> audiumEngine)
    : DocumentWindow (name,
                      juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                  .findColour (juce::ResizableWindow::backgroundColourId),
                      DocumentWindow::allButtons),
    audiumEngine(audiumEngine)
{
    setUsingNativeTitleBar (true);
    mainComponent.reset(new MainComponent(audiumEngine));
    setContentOwned (mainComponent.get(), true);
    
#if ! JUCE_MAC
    setMenuBar (AudiumApplication::getApp().getMenuModel());
#endif
    
    auto& commandManager = AudiumApplication::getCommandManager();

    auto registerAllAppCommands = [&]
    {
        commandManager.registerAllCommandsForTarget (this);
    };

    auto updateAppKeyMappings = [&]
    {
        commandManager.getKeyMappings()->resetToDefaultMappings();

        addKeyListener (commandManager.getKeyMappings());
    };

    
    registerAllAppCommands();
    updateAppKeyMappings();
    
    
#if JUCE_IOS || JUCE_ANDROID
    setFullScreen (true);
#else
    setResizable (true, true);
    
    String windowState;
    if (AudiumApplication::getPreferences().valueExists (PreferenceKeys::mainWindowState))
        windowState = AudiumApplication::getPreferences().getValue (PreferenceKeys::mainWindowState);

    if (windowState.isNotEmpty()) {
        restoreWindowStateFromString (windowState);
    }
    else {
        centreWithSize (getWidth(), getHeight());
    }
#endif

    mainComponent->updateUI();
    setVisible (true);
}

AudiumMainWindow::~AudiumMainWindow()
{
    AudiumApplication::getPreferences().setValue (PreferenceKeys::mainWindowState, getWindowStateAsString().toStdString());
#if ! JUCE_MAC
    setMenuBar (nullptr);
#endif
}

void AudiumMainWindow::closeButtonPressed()
{
    // This is called when the user tries to close this window. Here, we'll just
    // ask the app to quit when this happens, but you can change this to do
    // whatever you need.
    JUCEApplication::getInstance()->systemRequestedQuit();
}

//==============================================================================
ApplicationCommandTarget* AudiumMainWindow::getNextCommandTarget()
{
    return nullptr;
}

void AudiumMainWindow::getAllCommands (Array <CommandID>& commands)
{
    const CommandID ids[] =
    {
        CommandIDs::playStop,
        CommandIDs::loopPlayList,
        CommandIDs::createRegion,
        CommandIDs::splitRegion,
        CommandIDs::cleanupRegions,
        CommandIDs::autoEdit,
        CommandIDs::assembleSequential,
        CommandIDs::assembleRandom,
        CommandIDs::bounceProject,
        CommandIDs::duplicate,
        
        CommandIDs::createAudioTrack,
        
        CommandIDs::zoomIn,
        CommandIDs::zoomOut,
        CommandIDs::pageLeft,
        CommandIDs::pageRight,
        CommandIDs::followTransport,
        CommandIDs::toggleFullScreen,
        StandardApplicationCommandIDs::undo,
        StandardApplicationCommandIDs::redo,
        StandardApplicationCommandIDs::cut,
        StandardApplicationCommandIDs::copy,
        StandardApplicationCommandIDs::paste,
        StandardApplicationCommandIDs::del,
        StandardApplicationCommandIDs::selectAll
    };

    commands.addArray (ids, numElementsInArray (ids));
}

void AudiumMainWindow::getCommandInfo (const CommandID commandID, ApplicationCommandInfo& result)
{
    const int cmd = ModifierKeys::commandModifier;
    const int shift = ModifierKeys::shiftModifier;
    bool bActive = false;
    
    
    switch (commandID)
    {
        case CommandIDs::playStop:
            result.setInfo ("Play/Stop", "Play and stop", CommandCategories::transport, 0);
            result.defaultKeypresses.add (KeyPress (' ', ModifierKeys::noModifiers, 0));
            break;
        case CommandIDs::loopPlayList:
            result.setInfo ("Loop Playlist", "Loop Playlist On/Off", CommandCategories::transport, 0);
            result.defaultKeypresses.add (KeyPress ('l', ModifierKeys::ctrlModifier, 0));
            break;
        case CommandIDs::createRegion:
            result.setInfo ("Create Region...", "Creates a new region", CommandCategories::create, 0);
            result.setActive (getEngine()->getAudioTrackContainer()->getAudioRegionAdapter().canCreateRegion());
            result.defaultKeypresses.add (KeyPress ('r', ModifierKeys::commandModifier, 0));
            break;
        case CommandIDs::splitRegion:
            result.setInfo ("Split Region", "Splits a region", CommandCategories::editing, 0);
            if (getEngine()->getAudioTrackContainer()->getAudioRegionAdapter().anyRangeSelected()) {
                if (getEngine()->getAudioTrackContainer()->getAudioRegionAdapter().canCreateRegion())
                    bActive = true;
            }
            else {
                auto pos = getEngine()->getPlayListScheduler()->getAbsoluteStartPosition(audium::clocks);
                if (getEngine()->getAudioTrackContainer()->getAudioRegionAdapter().canSplitAnyRegionAtPosition(pos,audium::clocks))
                    bActive = true;
            }
            
            result.setActive (bActive);
            result.defaultKeypresses.add (KeyPress ('e', ModifierKeys::commandModifier, 0));
            break;
        case CommandIDs::cleanupRegions:
            result.setInfo ("Delete Unused Regions", "Deletes all unused regions", CommandCategories::editing, 0);
            break;
        case CommandIDs::autoEdit:
            result.setInfo ("1. Create Segments...", "Cuts the selected clip into segments at analysed boundaries", CommandCategories::editing, 0);
            result.setActive (canToggleAutoEditPreview());
            result.defaultKeypresses.add (KeyPress ('1', ModifierKeys::commandModifier, 0));
            break;
        case CommandIDs::assembleSequential:
            result.setInfo ("Sequential...", "Assembles a new sequence from the track's regions, in order", CommandCategories::editing, 0);
            result.setActive (canAssemble());
            result.defaultKeypresses.add (KeyPress ('2', ModifierKeys::commandModifier, 0));
            break;
        case CommandIDs::assembleRandom:
            result.setInfo ("Random...", "Assembles a new sequence from the track's regions, shuffled", CommandCategories::editing, 0);
            result.setActive (canAssemble());
            result.defaultKeypresses.add (KeyPress ('3', ModifierKeys::commandModifier, 0));
            break;
        case CommandIDs::bounceProject:
            result.setInfo ("Export Audio...", "Export current project as audio file", CommandCategories::general, 0);
            result.defaultKeypresses.add ({ 'b', ModifierKeys::commandModifier | ModifierKeys::altModifier, 0 });
            break;
        
        case CommandIDs::createAudioTrack:
            result.setInfo ("Create Audio Track...", "Create a new audio track", CommandCategories::create, 0);
            result.defaultKeypresses.add ({ 'n', ModifierKeys::commandModifier | ModifierKeys::shiftModifier, 0 });
            break;
        
        case CommandIDs::toggleFullScreen:
            result.setInfo ("Full Screen", "Enter full screen", CommandCategories::view, 0);
            result.defaultKeypresses.add (KeyPress ('f', ModifierKeys::commandModifier | ModifierKeys::ctrlModifier, 0));
            result.setTicked (isFullScreen());
            break;
        case CommandIDs::zoomIn:
            result.setInfo ("Zoom In", "Zoom in", CommandCategories::view, 0);
            result.defaultKeypresses.add (KeyPress ('+', ModifierKeys::commandModifier, 0));
            break;
        case CommandIDs::zoomOut:
            result.setInfo ("Zoom Out", "Zoom out", CommandCategories::view, 0);
            result.defaultKeypresses.add (KeyPress ('-', ModifierKeys::commandModifier, 0));
            break;
        case CommandIDs::pageLeft:
            result.setInfo ("Page Left", "Scroll one page feft", CommandCategories::view, 0);
            result.defaultKeypresses.add (KeyPress (KeyPress::leftKey, ModifierKeys::noModifiers, 0));
            break;
        case CommandIDs::pageRight:
            result.setInfo ("Page Right", "Scrool one page right", CommandCategories::view, 0);
            result.defaultKeypresses.add (KeyPress (KeyPress::rightKey, ModifierKeys::noModifiers, 0));
            break;
        case CommandIDs::followTransport:
            result.setInfo ("Follow Transport", "Follow Transport", CommandCategories::view, 0);
            result.defaultKeypresses.add (KeyPress ('f', ModifierKeys::ctrlModifier, 0));
            result.setTicked (getEngine()->getPlayListScheduler()->getFollowTransport());
            break;
        
        case StandardApplicationCommandIDs::undo:
            result.setInfo (TRANS ("Undo"), TRANS ("Undo"), "Editing", 0);
            result.setActive (getEngine()->getUndoManager()->canUndo());
            result.defaultKeypresses.add (KeyPress ('z', cmd, 0));
            break;

        case StandardApplicationCommandIDs::redo:
            result.setInfo (TRANS ("Redo"), TRANS ("Redo"), "Editing", 0);
            result.setActive (getEngine()->getUndoManager()->canRedo());
            result.defaultKeypresses.add (KeyPress ('z', cmd | shift, 0));
            break;

        case StandardApplicationCommandIDs::cut:
            result.setInfo (TRANS ("Cut"), String(), "Editing", 0);
            result.setActive (anythingSelected());
            result.defaultKeypresses.add (KeyPress ('x', cmd, 0));
            break;

        case StandardApplicationCommandIDs::copy:
            result.setInfo (TRANS ("Copy"), String(), "Editing", 0);
            result.setActive (anythingSelected());
            result.defaultKeypresses.add (KeyPress ('c', cmd, 0));
            break;

        case StandardApplicationCommandIDs::paste:
            result.setInfo (TRANS ("Paste"), String(), "Editing", 0);
            result.defaultKeypresses.add (KeyPress ('v', cmd, 0));
            result.setActive (canPaste());
            break;

        case StandardApplicationCommandIDs::del:
            result.setInfo (TRANS ("Delete"), String(), "Editing", 0);
            result.defaultKeypresses.add (KeyPress (KeyPress::deleteKey, ModifierKeys::noModifiers, 0));
            result.setActive (anythingSelected());
            break;

        case StandardApplicationCommandIDs::selectAll:
            result.setInfo (TRANS ("Select All"), String(), "Editing", 0);
            result.defaultKeypresses.add (KeyPress ('a', cmd, 0));
            break;

        case CommandIDs::duplicate:
            result.setInfo (TRANS ("Duplicate"), String(), "Editing", 0);
            result.defaultKeypresses.add (KeyPress ('d', cmd, 0));
            break;
            
        default:
            break;
    }
}

bool AudiumMainWindow::perform (const InvocationInfo& info)
{
    switch (info.commandID)
    {
        case CommandIDs::playStop:
            if (auto scheduler = getEngine()->getPlayListScheduler()) {
                scheduler->isPlaying() ? scheduler->stopPlaying() : scheduler->startPlaying();
            }
            mainComponent->updateUI();
            break;
        case CommandIDs::loopPlayList:
            notImplemented();
            break;
        case CommandIDs::createRegion:
            if (newRegionDialog == nullptr)
                newRegionDialog = std::make_unique<NewRegionDialog>();
            newRegionDialog->createNewRegion(getEngine());
            break;
        case CommandIDs::splitRegion:
            {
                auto pos = getEngine()->getPlayListScheduler()->getAbsoluteStartPosition(audium::clocks);
                getEngine()->getAudioTrackContainer()->getAudioRegionAdapter().splitRegions(pos, audium::clocks);
            }
            break;
        case CommandIDs::cleanupRegions:
            getEngine()->getAudioTrackContainer()->deleteUnusedRegions();
            break;
        case CommandIDs::autoEdit:
            toggleAutoEditPreview();
            break;
        case CommandIDs::assembleSequential:
            invokeAssemble(audium::AssembleConfig::Mode::Sequential);
            break;
        case CommandIDs::assembleRandom:
            invokeAssemble(audium::AssembleConfig::Mode::Random);
            break;
        case CommandIDs::bounceProject:
            if (exportAudioDialog == nullptr)
                exportAudioDialog = std::make_unique<ExportAudioDialog>(getEngine());
            exportAudioDialog->invoke (mainComponent);
            break;
        case CommandIDs::toggleFullScreen:
            setFullScreen (!isFullScreen());
            break;
        case CommandIDs::zoomIn:
            mainComponent->zoomIn();
            break;
        case CommandIDs::zoomOut:
            mainComponent->zoomOut();
            break;
        case CommandIDs::pageLeft:
            mainComponent->pageLeft();
            break;
        case CommandIDs::pageRight:
            mainComponent->pageRight();
            break;
        case CommandIDs::followTransport:
            getEngine()->getPlayListScheduler()->setFollowTransport(!getEngine()->getPlayListScheduler()->getFollowTransport());
            mainComponent->rebuildUI();
            break;
        case StandardApplicationCommandIDs::undo:
            getEngine()->getUndoManager()->undo();
            break;
        case StandardApplicationCommandIDs::redo:
            getEngine()->getUndoManager()->redo();
            break;
        case StandardApplicationCommandIDs::cut:
            mainComponent->copy();
            getEngine()->getAudioTrackContainer()->deleteSelectedObjects();
            break;
        case StandardApplicationCommandIDs::copy:
            mainComponent->copy();
            break;
        case StandardApplicationCommandIDs::paste:
            mainComponent->paste();
            break;
        case StandardApplicationCommandIDs::del:
            getEngine()->getAudioTrackContainer()->deleteSelectedObjects();
            break;
        case StandardApplicationCommandIDs::selectAll:
            mainComponent->selectAll();
            break;
        case CommandIDs::duplicate:
            mainComponent->duplicate();
            break;
        case CommandIDs::createAudioTrack:
            if (newAudioTrackDialog == nullptr)
                newAudioTrackDialog = std::make_unique<NewAudioTrackDialog>();
            newAudioTrackDialog->createNewAudioTrack(getEngine());
            break;
        default:
            return false;
    }

    return true;
}

bool AudiumMainWindow::anythingSelected()
{
    return getEngine()->getAudioTrackContainer()->getSelectionManager()->anythingSelected();
}

bool AudiumMainWindow::canPaste()
{
    return getEngine()->getAudioTrackContainer()->getSelectionManager()->canParseFromClipboard();
}

bool AudiumMainWindow::canToggleAutoEditPreview()
{
    auto analysisProvider = getEngine()->getAudioTrackContainer()->getAnalysisProvider();

    if (analysisProvider == nullptr)
        return false;

    // A pending edit can always be cancelled.
    if (analysisProvider->hasMergePreview())
        return true;

    audium::AutoEdit autoEdit(getEngine());

    audium::AutoEditConfig config;
    config.segmentMeasures = AutoEditOverlayControl::defaultMeasures;
    autoEdit.targetSelection(config);

    // Zero means no cut would land inside the clip - it is too short at this
    // measure value, and entering the edit would only dead-end at Apply.
    return autoEdit.resolveNumSegments(config) > 0;
}

void AudiumMainWindow::toggleAutoEditPreview()
{
    // The command system already refuses inactive commands; this guards any
    // other caller.
    if (! canToggleAutoEditPreview())
        return;

    auto analysisProvider = getEngine()->getAudioTrackContainer()->getAnalysisProvider();

    if (analysisProvider == nullptr)
        return;

    // Invoked while an edit is pending, the command cancels it - the cleared
    // preview hides the overlay control again.
    if (analysisProvider->hasMergePreview())
    {
        analysisProvider->clearMergePreview();
        return;
    }

    audium::AutoEdit autoEdit(getEngine());

    audium::AutoEditConfig config;
    config.segmentMeasures = AutoEditOverlayControl::defaultMeasures;
    autoEdit.targetSelection(config);

    if (! autoEdit.previewAutoEdit(config))
        NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::InfoIcon,
                                              "Auto Edit",
                                              "No preview is available for this clip yet - "
                                              "its analyses may still be running. Please try again shortly.");
}

bool AudiumMainWindow::canAssemble()
{
    audium::AutoEdit autoEdit(getEngine());

    audium::AssembleConfig config;
    autoEdit.targetAssembleTrack(config);

    auto track = getEngine()->getAudioTrackContainer()->getAudioTrack(config.trackId);

    if (track == nullptr)
        return false;

    // At least one region with audio is needed as source material.
    for (const auto& region : track->getRegions())
        if (region->getRegionData(audium::seconds).getLength() > 0.0)
            return true;

    return false;
}

void AudiumMainWindow::invokeAssemble(audium::AssembleConfig::Mode mode)
{
    if (assembleDialog == nullptr)
        assembleDialog = std::make_unique<AssembleDialog>();

    assembleDialog->assemble(getEngine(), mode);
}
