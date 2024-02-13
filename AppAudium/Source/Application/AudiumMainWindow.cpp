/*
  ==============================================================================

    AudiumMainWindow.cpp
    Created: 24 Mar 2023 10:51:48am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudiumMainWindow.h"
#include "AudiumApplication.h"
#include "AudiumCommandIDs.h"
#include "Util/EngineAccess.h"
#include "Engine/TransportSourceContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"

AudiumMainWindow::AudiumMainWindow (juce::String name, std::shared_ptr<AudiumEngine> audiumEngine)
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
 setMenuBar (ProjucerApplication::getApp().getMenuModel());
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
    centreWithSize (getWidth(), getHeight());
   #endif

    setVisible (true);
}

AudiumMainWindow::~AudiumMainWindow()
{
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
        CommandIDs::autoEdit,
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
        StandardApplicationCommandIDs::selectAll,
        StandardApplicationCommandIDs::deselectAll
    };

    commands.addArray (ids, numElementsInArray (ids));
}

void AudiumMainWindow::getCommandInfo (const CommandID commandID, ApplicationCommandInfo& result)
{
    const int cmd = ModifierKeys::commandModifier;
    const int shift = ModifierKeys::shiftModifier;
    
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
            result.defaultKeypresses.add (KeyPress ('r', ModifierKeys::commandModifier, 0));
            break;
        case CommandIDs::autoEdit:
            result.setInfo ("Auto Edit", "Automatically creates an Edit", CommandCategories::editing, 0);
            result.defaultKeypresses.add (KeyPress ('e', ModifierKeys::commandModifier, 0));
            break;
        case CommandIDs::toggleFullScreen:
            result.setInfo ("Full Screen", "Enter full screen", CommandCategories::view, 0);
            result.defaultKeypresses.add (KeyPress ('f', ModifierKeys::commandModifier | ModifierKeys::ctrlModifier, 0));
            break;
        case CommandIDs::toggleEditArrangement:
            result.setInfo ("Toggle Edit/Arrangement View", "Toggle Edit/Arrangement View", CommandCategories::view, 0);
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
            // TODO: result.setActive (isSomethingSelected());
            result.defaultKeypresses.add (KeyPress ('x', cmd, 0));
            break;

        case StandardApplicationCommandIDs::copy:
            result.setInfo (TRANS ("Copy"), String(), "Editing", 0);
            // TODO: result.setActive (isSomethingSelected());
            result.defaultKeypresses.add (KeyPress ('c', cmd, 0));
            break;

        case StandardApplicationCommandIDs::paste:
            {
                result.setInfo (TRANS ("Paste"), String(), "Editing", 0);
                result.defaultKeypresses.add (KeyPress ('v', cmd, 0));

                bool canPaste = false;

                // TODO:
//                if (auto doc = parseXML (SystemClipboard::getTextFromClipboard()))
//                {
//                    if (doc->hasTagName (ComponentLayout::clipboardXmlTag))
//                        canPaste = (currentLayout != nullptr);
//                    else if (doc->hasTagName (PaintRoutine::clipboardXmlTag))
//                        canPaste = (currentPaintRoutine != nullptr);
//                }

                result.setActive (canPaste);
            }

            break;

        case StandardApplicationCommandIDs::del:
            result.setInfo (TRANS ("Delete"), String(), "Editing", 0);
            // TODO: result.setActive (isSomethingSelected());
            break;

        case StandardApplicationCommandIDs::selectAll:
            result.setInfo (TRANS ("Select All"), String(), "Editing", 0);
            // TODO: result.setActive (currentPaintRoutine != nullptr || currentLayout != nullptr);
            result.defaultKeypresses.add (KeyPress ('a', cmd, 0));
            break;

        case StandardApplicationCommandIDs::deselectAll:
            result.setInfo (TRANS ("Deselect All"), String(), "Editing", 0);
            // TODO: result.setActive (currentPaintRoutine != nullptr || currentLayout != nullptr);
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
            getEngine()->getPlayListScheduler()->isPlaying() ?
                getEngine()->getPlayListScheduler()->stopPlaying() :
                getEngine()->getPlayListScheduler()->startPlaying();
            mainComponent->updateUI();
            break;
        case CommandIDs::loopPlayList:
            getEngine()->getPlayListScheduler()->setLoopPlayList(!getEngine()->getPlayListScheduler()->getLoopPlayList());
            break;
        case CommandIDs::createRegion:
            newRegionDialog.createNewRegion(getEngine());
            break;
        case CommandIDs::autoEdit:
            autoEditDialog.invokeAutoEdit(getEngine(), mainComponent);
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
//            document->dispatchPendingMessages();
            break;

        case StandardApplicationCommandIDs::redo:
            getEngine()->getUndoManager()->redo();
//            document->dispatchPendingMessages();
            break;
        case StandardApplicationCommandIDs::cut:
            notImplemented();
//            if (currentLayout != nullptr)
//            {
//                currentLayout->copySelectedToClipboard();
//                currentLayout->deleteSelected();
//            }
//            else if (currentPaintRoutine != nullptr)
//            {
//                currentPaintRoutine->copySelectedToClipboard();
//                currentPaintRoutine->deleteSelected();
//            }

            break;

        case StandardApplicationCommandIDs::copy:
            notImplemented();
//            if (currentLayout != nullptr)
//                currentLayout->copySelectedToClipboard();
//            else if (currentPaintRoutine != nullptr)
//                currentPaintRoutine->copySelectedToClipboard();

            break;

        case StandardApplicationCommandIDs::paste:
            notImplemented();
//            {
//                if (auto doc = parseXML (SystemClipboard::getTextFromClipboard()))
//                {
//                    if (doc->hasTagName (ComponentLayout::clipboardXmlTag))
//                    {
//                        if (currentLayout != nullptr)
//                            currentLayout->paste();
//                    }
//                    else if (doc->hasTagName (PaintRoutine::clipboardXmlTag))
//                    {
//                        if (currentPaintRoutine != nullptr)
//                            currentPaintRoutine->paste();
//                    }
//                }
//            }
            break;

        case StandardApplicationCommandIDs::del:
            notImplemented();
//            if (currentLayout != nullptr)
//                currentLayout->deleteSelected();
//            else if (currentPaintRoutine != nullptr)
//                currentPaintRoutine->deleteSelected();
            break;

        case StandardApplicationCommandIDs::selectAll:
            notImplemented();
//            if (currentLayout != nullptr)
//                currentLayout->selectAll();
//            else if (currentPaintRoutine != nullptr)
//                currentPaintRoutine->selectAll();
            break;

        case StandardApplicationCommandIDs::deselectAll:
            notImplemented();
//            if (currentLayout != nullptr)
//            {
//                currentLayout->getSelectedSet().deselectAll();
//            }
//            else if (currentPaintRoutine != nullptr)
//            {
//                currentPaintRoutine->getSelectedElements().deselectAll();
//                currentPaintRoutine->getSelectedPoints().deselectAll();
//            }

            break;
            
        default:
            return false;
    }

    return true;
}
