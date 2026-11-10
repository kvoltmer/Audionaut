//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudiumMainWindow.h"
#include "AudiumApplication.h"
#include "AudiumCommandIDs.h"
#include "Util/EngineAccess.h"
#include "Util/Preferences.h"
#include "Engine/AudioSources/VoiceSourceContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/ActionMessages.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Undo/UndoableEdit.h"
#include "Engine/Region/AudioRegion.h"
#include "Interface/Dialogs/NewRegionDialog.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/AutoEdit/AutoEdit.h"
#include "Interface/Controls/AutoEditOverlayControl.h"
#include "Interface/Dialogs/ExportAudioDialog.h"
#include "Interface/Dialogs/NewAudioTrackDialog.h"
#include "Interface/Dialogs/AssembleDialog.h"
#include "Interface/Dialogs/StemSeparationDialog.h"
#include "Engine/Separation/DemucsConfig.h"
#include "Interface/Dialogs/AnalysisSettingsComponent.h"
#include "Interface/Handlers/SnapToGridHandler.h"

using namespace audium;

namespace {

// Why toggling the preview showed nothing, precisely enough to act on: an
// analysis switched off in the settings never finishes by waiting, and only
// "still analysing" is cured by trying again.
juce::String describeUnavailablePreview(audium::AutoEdit& autoEdit,
                                        audium::AutoEditConfig& config)
{
    const auto switchedOff = autoEdit.findSwitchedOffMergeAnalyses(config);

    if (! switchedOff.empty())
    {
        juce::StringArray names;

        for (auto analysisType : switchedOff)
            names.add(AnalysisSettingsComponent::displayName(analysisType));

        return "No preview can be prepared for this clip: the "
               + names.joinIntoString(" and ")
               + (names.size() > 1 ? " analyses are" : " analysis is")
               + " switched off. Enable it on the Analysis tab of the "
                 "settings, then add the clip's audio file again to analyse it.";
    }

    if (! autoEdit.isAnalysisDone(config))
        return "This clip is still being analysed. The preview will be "
               "available once its analyses have finished - please try again shortly.";

    return "No preview is available for this clip - its audio file could not be read.";
}

} // namespace

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
    
    commands = makeCommands();

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
    // narrower than this and the arrangement would be squeezed under its minimum by an open right panel
    setResizeLimits (MainComponent::minimumWidth, MainComponent::minimumHeight, 32768, 32768);

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

std::vector<AudiumMainWindow::Command> AudiumMainWindow::makeCommands()
{
    const int cmd = ModifierKeys::commandModifier;
    const int shift = ModifierKeys::shiftModifier;
    const int none = ModifierKeys::noModifiers;

    auto regions = [this]() -> audium::AudioRegionAdapter& {
        return getEngine()->getAudioTrackContainer()->getAudioRegionAdapter();
    };

    // The order is getAllCommands()' order.
    return {
        { CommandIDs::playStop, "Play/Stop", "Play and stop", CommandCategories::transport,
          { KeyPress (' ', none, 0) }, {}, {}, {},
          [this] {
              if (auto scheduler = getEngine()->getPlayListScheduler()) {
                  scheduler->isPlaying() ? scheduler->stopPlaying() : scheduler->startPlaying();
              }
              mainComponent->updateUI();
          } },
        { CommandIDs::loopSelection, "Loop Selection", "Sets the loop range to the selected range and turns the loop on", CommandCategories::transport,
          { KeyPress ('l', cmd, 0) },
          [regions] { return regions().anyRangeSelected(); }, {}, {},
          [this] { loopSelectedRange(); } },
        { CommandIDs::createRegion, "Create Region...", "Creates a new region", CommandCategories::create,
          { KeyPress ('r', cmd, 0) },
          [regions] { return regions().canCreateRegion(); }, {}, {},
          [this] {
              if (newRegionDialog == nullptr)
                  newRegionDialog = std::make_unique<NewRegionDialog>();
              newRegionDialog->createNewRegion(getEngine());
          } },
        { CommandIDs::splitRegion, "Split", "Splits a region", CommandCategories::editing,
          { KeyPress ('e', cmd, 0) },
          [this, regions] {
              if (regions().anyRangeSelected())
                  return regions().canCreateRegion();

              auto pos = getEngine()->getPlayListScheduler()->getAbsoluteStartPosition(audium::clocks);
              return regions().canSplitAnyRegionAtPosition(pos, audium::clocks);
          }, {}, {},
          [this, regions] {
              auto pos = getEngine()->getPlayListScheduler()->getAbsoluteStartPosition(audium::clocks);
              regions().splitRegions(pos, audium::clocks);
          } },
        { CommandIDs::cleanupRegions, "Delete Unused Regions", "Deletes all unused regions", CommandCategories::editing,
          {}, {}, {}, {},
          [this] { getEngine()->getAudioTrackContainer()->deleteUnusedRegions(); } },
        { CommandIDs::autoEdit, "1. Create Segments...", "Cuts the selected clip into segments at analysed boundaries", CommandCategories::editing,
          { KeyPress ('1', cmd, 0) },
          [this] { return canToggleAutoEditPreview(); }, {}, {},
          [this] { toggleAutoEditPreview(); } },
        { CommandIDs::assembleSequential, "Sequential...", "Assembles a new sequence from the track's regions, in order", CommandCategories::editing,
          { KeyPress ('2', cmd, 0) },
          [this] { return canAssemble(); }, {}, {},
          [this] { invokeAssemble(audium::AssembleConfig::Mode::Sequential); } },
        { CommandIDs::assembleRandom, "Random...", "Assembles a new sequence from the track's regions, shuffled", CommandCategories::editing,
          { KeyPress ('3', cmd, 0) },
          [this] { return canAssemble(); }, {}, {},
          [this] { invokeAssemble(audium::AssembleConfig::Mode::Random); } },
        { CommandIDs::separateStems, "Separate Stems...", "Splits the selected clip into Drums, Bass, Other and Vocals tracks", CommandCategories::editing,
          {},
          [this] { return canSeparateStems(); }, {}, {},
          [this] { invokeSeparateStems(); } },
        { CommandIDs::stretchClip, "Stretch Clip...", "Adjusts the selected clip's playback speed (re-pitch)", CommandCategories::editing,
          { KeyPress ('t', cmd, 0) },
          [this] { return canToggleStretchOverlay(); }, {}, {},
          [this] { toggleStretchOverlay(); } },
        { CommandIDs::bounceProject, "Export Audio...", "Export current project as audio file", CommandCategories::general,
          { KeyPress ('b', cmd | ModifierKeys::altModifier, 0) }, {}, {}, {},
          [this] {
              if (exportAudioDialog == nullptr)
                  exportAudioDialog = std::make_unique<ExportAudioDialog>(getEngine());
              exportAudioDialog->invoke (mainComponent);
          } },
        { CommandIDs::duplicate, TRANS ("Duplicate"), String(), CommandCategories::editing,
          { KeyPress ('d', cmd, 0) }, {}, {}, {},
          [this] { mainComponent->duplicate(); } },

        { CommandIDs::createAudioTrack, "Create Audio Track...", "Create a new audio track", CommandCategories::create,
          { KeyPress ('n', cmd | shift, 0) }, {}, {}, {},
          [this] {
              if (newAudioTrackDialog == nullptr)
                  newAudioTrackDialog = std::make_unique<NewAudioTrackDialog>();
              newAudioTrackDialog->createNewAudioTrack(getEngine());
          } },
        { CommandIDs::createAuxBus, "Create Aux Bus", "Adds an aux bus; every channel gets a send into it", CommandCategories::create,
          {}, {}, {}, {},
          [this] {
              auto& container = *getEngine()->getAudioTrackContainer();
              audium::applyAsUndoableEdit (container, [&] { return container.addAuxBus() != nullptr; }, "Add Aux Bus", false);
          } },

        { CommandIDs::zoomIn, "Zoom In", "Zoom in", CommandCategories::view,
          { KeyPress ('+', cmd, 0) }, {}, {}, {},
          [this] { mainComponent->zoomIn(); } },
        { CommandIDs::zoomOut, "Zoom Out", "Zoom out", CommandCategories::view,
          { KeyPress ('-', cmd, 0) }, {}, {}, {},
          [this] { mainComponent->zoomOut(); } },
        { CommandIDs::pageLeft, "Page Left", "Scroll one page left", CommandCategories::view,
          { KeyPress (KeyPress::leftKey, none, 0) }, {}, {}, {},
          [this] { mainComponent->pageLeft(); } },
        { CommandIDs::pageRight, "Page Right", "Scroll one page right", CommandCategories::view,
          { KeyPress (KeyPress::rightKey, none, 0) }, {}, {}, {},
          [this] { mainComponent->pageRight(); } },
        { CommandIDs::followTransport, "Follow Transport", "Follow Transport", CommandCategories::view,
          { KeyPress ('f', ModifierKeys::ctrlModifier, 0) },
          {}, [this] { return getEngine()->getPlayListScheduler()->getFollowTransport(); }, {},
          [this] {
              getEngine()->getPlayListScheduler()->setFollowTransport(!getEngine()->getPlayListScheduler()->getFollowTransport());
              mainComponent->rebuildUI();
          } },
        { CommandIDs::enableSnapToGrid, "Snap to Grid", "Snap drags to the grid", CommandCategories::view,
          { KeyPress ('g', cmd, 0) },
          {}, [] { return SnapToGridHandler::isEnabled(); }, {},
          [] {
              SnapToGridHandler::setEnabled (! SnapToGridHandler::isEnabled());
              AudiumApplication::getPreferences().setValue (PreferenceKeys::snapToGridEnabled,
                                                            SnapToGridHandler::isEnabled() ? "true" : "false");
          } },
        { CommandIDs::toggleFullScreen, "Full Screen", "Enter full screen", CommandCategories::view,
          { KeyPress ('f', cmd | ModifierKeys::ctrlModifier, 0) },
          {}, [this] { return isFullScreen(); }, {},
          [this] { setFullScreen (!isFullScreen()); } },
        { CommandIDs::showMixer, "Mixer", "Shows the mixer below the arrangement", CommandCategories::view,
          { KeyPress ('m', cmd | ModifierKeys::shiftModifier, 0) },
          {}, [this] { return mainComponent->isMixerVisible(); }, {},
          [this] { mainComponent->setMixerVisible (! mainComponent->isMixerVisible()); } },
        { CommandIDs::showAuxBuses, "Aux Buses", "Shows the aux bus strips in the mixer's bus section", CommandCategories::view,
          {},
          {}, [this] { return mainComponent->areAuxBusesVisible(); }, {},
          [this] { mainComponent->setAuxBusesVisible (! mainComponent->areAuxBusesVisible()); } },
        { CommandIDs::showRightPanel, "Right Panel", "Shows the playlist and region panel right of the arrangement", CommandCategories::view,
          { KeyPress ('p', cmd | ModifierKeys::shiftModifier, 0) },
          {}, [this] { return mainComponent->isRightPanelVisible(); }, {},
          [this] { mainComponent->setRightPanelVisible (! mainComponent->isRightPanelVisible()); } },

        // The step's name follows the verb, as the undo history names it:
        // "Undo Move Clip", "Undo Agent: remove pauses". The menu is built
        // when it opens, so the label is current.
        { StandardApplicationCommandIDs::undo, TRANS ("Undo"), TRANS ("Undo"), CommandCategories::editing,
          { KeyPress ('z', cmd, 0) },
          [this] { return getEngine()->getUndoManager()->canUndo(); }, {},
          [this] {
              const auto step = getEngine()->getUndoManager()->getUndoDescription();
              return step.isEmpty() ? TRANS ("Undo") : TRANS ("Undo") + " " + step;
          },
          [this] { getEngine()->getUndoManager()->undo(); } },
        { StandardApplicationCommandIDs::redo, TRANS ("Redo"), TRANS ("Redo"), CommandCategories::editing,
          { KeyPress ('z', cmd | shift, 0) },
          [this] { return getEngine()->getUndoManager()->canRedo(); }, {},
          [this] {
              const auto step = getEngine()->getUndoManager()->getRedoDescription();
              return step.isEmpty() ? TRANS ("Redo") : TRANS ("Redo") + " " + step;
          },
          [this] { getEngine()->getUndoManager()->redo(); } },
        { StandardApplicationCommandIDs::cut, TRANS ("Cut"), String(), CommandCategories::editing,
          { KeyPress ('x', cmd, 0) },
          [this] { return anythingSelected(); }, {}, {},
          [this] {
              mainComponent->copy();
              getEngine()->getAudioTrackContainer()->deleteSelectedObjects();
          } },
        { StandardApplicationCommandIDs::copy, TRANS ("Copy"), String(), CommandCategories::editing,
          { KeyPress ('c', cmd, 0) },
          [this] { return anythingSelected(); }, {}, {},
          [this] { mainComponent->copy(); } },
        { StandardApplicationCommandIDs::paste, TRANS ("Paste"), String(), CommandCategories::editing,
          { KeyPress ('v', cmd, 0) },
          [this] { return canPaste(); }, {}, {},
          [this] { mainComponent->paste(); } },
        { StandardApplicationCommandIDs::del, TRANS ("Delete"), String(), CommandCategories::editing,
          // the Mac "delete" key: reaches the command whenever the focused
          // component (a clip body, an overlay, ...) doesn't handle it itself
          { KeyPress (KeyPress::deleteKey, none, 0), KeyPress (KeyPress::backspaceKey, none, 0) },
          [this] { return anythingSelected(); }, {}, {},
          [this] { getEngine()->getAudioTrackContainer()->deleteSelectedObjects(); } },
        { StandardApplicationCommandIDs::selectAll, TRANS ("Select All"), String(), CommandCategories::editing,
          { KeyPress ('a', cmd, 0) }, {}, {}, {},
          [this] { mainComponent->selectAll(); } },
    };
}

const AudiumMainWindow::Command* AudiumMainWindow::findCommand (CommandID commandID) const
{
    for (const auto& command : commands)
        if (command.id == commandID)
            return &command;

    return nullptr;
}

void AudiumMainWindow::getAllCommands (Array <CommandID>& result)
{
    for (const auto& command : commands)
        result.add (command.id);
}

void AudiumMainWindow::getCommandInfo (const CommandID commandID, ApplicationCommandInfo& result)
{
    auto command = findCommand (commandID);
    if (command == nullptr)
        return;

    result.setInfo (command->currentName ? command->currentName() : command->name,
                    command->description, command->category, 0);
    if (command->isActive)
        result.setActive (command->isActive());
    if (command->isTicked)
        result.setTicked (command->isTicked());
    result.defaultKeypresses.addArray (command->keys);
}

bool AudiumMainWindow::perform (const InvocationInfo& info)
{
    auto command = findCommand (info.commandID);
    if (command == nullptr)
        return false;

    command->perform();
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
    {
        NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::InfoIcon,
                                              "Auto Edit",
                                              describeUnavailablePreview(autoEdit, config));
        return;
    }

    // Select the track the segments belong to, so its highlight in the
    // channel and playlist headers shows where the edit lands.
    if (auto track = getEngine()->getAudioTrackContainer()->getAudioTrack(config.trackId))
    {
        getEngine()->getAudioTrackContainer()->getSelectionManager()->deselectAll();
        track->setSelected(true);
        getEngine()->getAudioTrackContainer()->sendActionMessage(audium::updateAll);
    }
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

bool AudiumMainWindow::canSeparateStems()
{
#if ! DEMUCS_ENABLED
    return false;
#else
    if (getEngine()->getPlayListScheduler()->isPlaying())
        return false;

    auto selectedClips = 0;

    for (const auto& object : getEngine()->getAudioTrackContainer()->getSelectionManager()->getSelectedObjects())
        if (dynamic_cast<audium::PlayListItem*>(object.get()) != nullptr)
            ++selectedClips;

    return selectedClips == 1;
#endif
}

void AudiumMainWindow::invokeSeparateStems()
{
    if (stemSeparationDialog == nullptr)
        stemSeparationDialog = std::make_unique<StemSeparationDialog>();

    stemSeparationDialog->separate(getEngine(), mainComponent.get());
}

bool AudiumMainWindow::canToggleStretchOverlay()
{
    auto target = getEngine()->getAudioTrackContainer()->getClipOverlayTarget();

    // an open overlay can always be dismissed
    if (target->isActive())
        return true;

    auto item = getEngine()->getAudioTrackContainer()->getSelectionManager()->firstSelectedClip();
    return item != nullptr && ! item->isRecording();
}

void AudiumMainWindow::loopSelectedRange()
{
    auto container = getEngine()->getAudioTrackContainer();
    auto range = container->getAudioRegionAdapter().getSelectedRange(audium::clocks);
    if (range.isEmpty())
        return;

    auto transportLoop = getEngine()->getPlayListScheduler()->getTransportLoop();
    transportLoop->setLoopPositionRange(container, range, audium::clocks);
    transportLoop->setLoopActive(true);

    // same refresh the header's loop button triggers
    container->sendActionMessage(audium::updateArrangementAction);
    mainComponent->updateUI();
}

void AudiumMainWindow::toggleStretchOverlay()
{
    auto target = getEngine()->getAudioTrackContainer()->getClipOverlayTarget();

    if (target->isActive())
    {
        // dismissing commits the overlay's pending edit (see
        // StretchOverlayControl::visibilityChanged)
        target->clear();
        return;
    }

    if (auto item = getEngine()->getAudioTrackContainer()->getSelectionManager()->firstSelectedClip())
        target->set(item->getRegion()->getAudioTrack()->getId(), item->getId());
}
