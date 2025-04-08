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
#include "Engine/Group/AudioTrackContainer.h"
#include "Interface/Dialogs/NewRegionDialog.h"
#include "Interface/Dialogs/AutoEditDialog.h"
#include "Interface/Dialogs/ExportAudioDialog.h"

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
    if (Preferences::valueExists (PreferenceKeys::mainWindowState))
        windowState = Preferences::getValue (PreferenceKeys::mainWindowState);

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
    Preferences::setValue (PreferenceKeys::mainWindowState, getWindowStateAsString());
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
        CommandIDs::bounceProject,
        CommandIDs::duplicate,
        CommandIDs::zoomIn,
        CommandIDs::zoomOut,
        CommandIDs::pageLeft,
        CommandIDs::pageRight,
        CommandIDs::followTransport,
        CommandIDs::toggleEditArrangement,
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
            result.setInfo ("Create Region", "Creates a new region", CommandCategories::editing, 0);
            result.setActive (getEngine()->getAudioTrackContainer()->getAudioRegionAdapter().anyRangeSelected());
            result.defaultKeypresses.add (KeyPress ('r', ModifierKeys::commandModifier, 0));
            break;
        case CommandIDs::splitRegion:
            result.setInfo ("Split Region", "Splits a region", CommandCategories::editing, 0);
            if (getEngine()->getAudioTrackContainer()->getAudioRegionAdapter().anyRangeSelected() &&
                getEngine()->getPlayListScheduler()->isArrangementMode()) {
                bActive = true;
            }
            result.setActive (bActive);
            result.defaultKeypresses.add (KeyPress ('e', ModifierKeys::commandModifier, 0));
            break;
        case CommandIDs::cleanupRegions:
            result.setInfo ("Delete Unused Regions", "Deletes all unused regions", CommandCategories::editing, 0);
            break;
        case CommandIDs::autoEdit:
            result.setInfo ("Auto Edit", "Automatically creates an Edit", CommandCategories::editing, 0);
            result.defaultKeypresses.add (KeyPress ('t', ModifierKeys::commandModifier, 0));
            break;
        case CommandIDs::bounceProject:
            result.setInfo ("Export Audio...", "Export current project as audio file", CommandCategories::general, 0);
            result.defaultKeypresses.add ({ 'b', ModifierKeys::commandModifier | ModifierKeys::altModifier, 0 });
            break;
        case CommandIDs::toggleFullScreen:
            result.setInfo ("Full Screen", "Enter full screen", CommandCategories::view, 0);
            result.defaultKeypresses.add (KeyPress ('f', ModifierKeys::commandModifier | ModifierKeys::ctrlModifier, 0));
            break;
        case CommandIDs::toggleEditArrangement:
            result.setInfo ("Toggle Region / Arrangement View", "Toggle Region / Arrangement View", CommandCategories::view, 0);
            result.defaultKeypresses.add (KeyPress (KeyPress::tabKey, ModifierKeys::noModifiers, 0));
            break;
        case CommandIDs::zoomIn:
            result.setInfo ("Zoom In", "Zoom in", CommandCategories::view, 0);
            result.defaultKeypresses.add (KeyPress ('+', ModifierKeys::ctrlModifier, 0));
            break;
        case CommandIDs::zoomOut:
            result.setInfo ("Zoom Out", "Zoom out", CommandCategories::view, 0);
            result.defaultKeypresses.add (KeyPress ('-', ModifierKeys::ctrlModifier, 0));
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
            result.setActive (isSomethingSelected());
            result.defaultKeypresses.add (KeyPress ('x', cmd, 0));
            break;

        case StandardApplicationCommandIDs::copy:
            result.setInfo (TRANS ("Copy"), String(), "Editing", 0);
            result.setActive (isSomethingSelected());
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
            result.setActive (isSomethingSelected());
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
            getEngine()->getAudioTrackContainer()->getAudioRegionAdapter().splitRegionsFromSelection();
            break;
        case CommandIDs::cleanupRegions:
            getEngine()->getAudioTrackContainer()->deleteUnusedRegions();
            break;
        case CommandIDs::autoEdit:
            if (autoEditDialog == nullptr)
                autoEditDialog = std::make_unique<AutoEditDialog>();
            autoEditDialog->invokeAutoEdit(getEngine(), mainComponent);
            break;
        case CommandIDs::bounceProject:
            if (exportAudioDialog == nullptr)
                exportAudioDialog = std::make_unique<ExportAudioDialog>(getEngine());
            exportAudioDialog->invoke(mainComponent);
            break;
        case CommandIDs::toggleFullScreen:
            setFullScreen (!isFullScreen());
            break;
        case CommandIDs::toggleEditArrangement:
            mainComponent->toggleEditArrangementComponent();
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
            
        default:
            return false;
    }

    return true;
}

bool AudiumMainWindow::isSomethingSelected()
{
    return getEngine()->getAudioTrackContainer()->getSelectionManager()->isSomethingSelected();
}

bool AudiumMainWindow::canPaste()
{
    return getEngine()->getAudioTrackContainer()->getSelectionManager()->canParseFromClipboard();
}
