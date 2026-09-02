//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudiumApplication.h"
#include "Cli/CliContext.h"
#include "Cli/CliDispatch.h"
#include "Cli/HeadlessEngineSession.h"
#include "Interface/Components/MainComponent.h"
#include "Engine/Core/HeadlessMode.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Factory/AudiumFactory.h"
#include "Application/AudiumMenuModel.h"
#include "Application/ProjectMonitor.h"
#include "Util/EngineAccess.h"
#include "Util/Preferences.h"
#include "UsageAnalytics.h"
#include "AudiumMainWindow.h"
#include "Interface/Dialogs/ProjectSettingsComponent.h"
#include "Interface/Dialogs/SettingsDialog.h"
#include "Interface/Dialogs/FloatingToolWindow.h"
#include "Interface/Dialogs/FileBrowserView.h"
#include "Interface/Dialogs/AboutWindowComponent.h"

using namespace audium;

AudiumApplication::AudiumApplication() = default;
AudiumApplication::~AudiumApplication() = default;

static void logAppLaunch(audium::UsageAnalytics& analytics)
{
    StringPairArray params;
    params.set("app_version", JUCEApplication::getInstance()->getApplicationVersion());
    params.set("os", SystemStats::getOperatingSystemName());
    analytics.logEvent("app_launch", params);
}

AudiumApplication& AudiumApplication::getApp()
{
    AudiumApplication* const app = dynamic_cast<AudiumApplication*> (JUCEApplication::getInstance());
    jassert (app != nullptr);
    return *app;
}

juce::ApplicationCommandManager& AudiumApplication::getCommandManager()
{
    auto* cm = AudiumApplication::getApp().commandManager.get();
    jassert (cm != nullptr);
    return *cm;
}

audium::Preferences& AudiumApplication::getPreferences()
{
    auto* prefs = AudiumApplication::getApp().preferences.get();
    jassert (prefs != nullptr);
    return *prefs;
}

// The app is a GUI-subsystem executable on Windows, so stdout/stderr are
// disconnected when run from a terminal - the CLI output would silently
// vanish without re-attaching to the parent console. Known caveat: the shell
// prompt returns immediately (shells don't wait on GUI processes), so output
// interleaves with it; audionaut-cli is the clean console-subsystem option.
static void attachToParentConsoleForCli()
{
#if JUCE_WINDOWS
    if (AttachConsole (ATTACH_PARENT_PROCESS))
    {
        FILE* unused = nullptr;
        freopen_s (&unused, "CONOUT$", "w", stdout);
        freopen_s (&unused, "CONOUT$", "w", stderr);
        std::ios::sync_with_stdio();
    }
#endif
}

void AudiumApplication::initialise (const juce::String& commandLine)
{
    // Preferences first: the CLI path below may need them and they have no
    // window dependency (mirrors Projucer's settings-before-CLI ordering).
    initPreferences();

    // Projucer-style in-app CLI: a recognised verb runs headlessly and the
    // process quits with its exit code; anything else (a project file, no
    // arguments) falls through to normal GUI startup.
    isRunningCommandLine = commandLine.trim().isNotEmpty()
                            && ! commandLine.startsWith ("-NSDocumentRevisionsDebugMode");

    if (isRunningCommandLine)
    {
        attachToParentConsoleForCli();
        HeadlessMode::set (true);
        cli::HeadlessEngineSession::setUseExternalMessageManager (true);

        juce::ArgumentList args ("Audionaut", commandLine);
        cli::CliContext context; // captures the real stdout before any redirect
        context.json = args.removeOptionIfFound ("--json");
        context.quiet = args.removeOptionIfFound ("--quiet");
        context.preferences = &getPreferences(); // consent-gated CLI analytics

        const auto exitCode = cli::performCliCommand (args, context);
        if (exitCode != cli::cliCommandNotPerformed)
        {
            setApplicationReturnValue (exitCode);
            quit();
            return; // no window, splash, engine or analytics were created
        }

        isRunningCommandLine = false;
        HeadlessMode::set (false);
        cli::HeadlessEngineSession::setUseExternalMessageManager (false);
    }

    // Single-instance behavior, handled manually after the CLI block (see
    // moreThanOneInstanceAllowed): a second GUI launch forwards its command
    // line to the running instance and quits.
    if (sendCommandLineToPreexistingInstance())
    {
        quit();
        return;
    }

    LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    splashScreen = std::make_unique<AboutSplashScreen>();

    initCommandManager();

    usageAnalytics = std::make_unique<UsageAnalytics>(getPreferences());
    if (UsageAnalytics::isConsentDecided(getPreferences()))
        logAppLaunch(*usageAnalytics);
    // otherwise handleAsyncUpdate asks for consent and logs the launch then


    // create audium engine
    audiumEngine = audium::AudiumFactory::createAudiumEngine();
    fileStore = audiumEngine->getProjectFileStore();
    serializer = audiumEngine->getProjectSerializer();
    audiumEngine->initialise();
    applyAnalysisPreferences();


    initialOpenDirectory = File::getSpecialLocation (File::userMusicDirectory);
    initialSaveDirectory = File(File::getSpecialLocation (File::userMusicDirectory).getFullPathName() +
                                File::getSeparatorString() +
                                getApplicationName());
    if (initialSaveDirectory.exists() == false) {
        initialSaveDirectory.createDirectory();
    }
        
    
    if (getPreferences().valueExists(PreferenceKeys::initialOpenDirectory))
        initialOpenDirectory = juce::File(getPreferences().getValue(PreferenceKeys::initialOpenDirectory));
    
    if (getPreferences().valueExists(PreferenceKeys::initialSaveDirectory))
        initialSaveDirectory = juce::File(getPreferences().getValue(PreferenceKeys::initialSaveDirectory));
    
    
    
    // try to load project from command line
    if (! commandLine.trim().startsWithChar ('-')) {
        ArgumentList list ({}, commandLine);

        for (auto& arg : list.arguments) {
            openFile (arg.resolveAsFile());
        }
    }
    


    
    // do further initialisation in a moment when the message loop has started
    triggerAsyncUpdate();
}

void AudiumApplication::handleAsyncUpdate()
{
    menuModel = std::make_unique<AudiumMenuModel>();

#if JUCE_MAC
    rebuildAppleMenu();
    appleMenuRebuildListener = std::make_unique<AppleMenuRebuildListener>();
#endif
    mainWindow.reset (new AudiumMainWindow (getApplicationName(), audiumEngine));

    if (getPreferences().getValue(PreferenceKeys::browserWindowOpen) == "true")
        showFileBrowserWindow();

    offerOrphanedTempProjectRestore();
    loadStartupProject();

    updateUI();
    refreshWindowTitle();
    startProjectMonitor();

    if (splashScreen != nullptr) {
        splashScreen->deleteAfterDelay (RelativeTime::seconds (0.5), true);
        splashScreen.release(); // SplashScreen deletes itself (DeletedAtShutdown) once the delay/click fires
    }

    askForUsageStatisticsConsent();
}

void AudiumApplication::loadStartupProject()
{
    // try to load default project from prefs
    if (!fileStore->getCurrentProjectFile().exists() &&
        getPreferences().valueExists(PreferenceKeys::defaultFile))
        openFile(juce::File(getPreferences().getValue(PreferenceKeys::defaultFile)));

    // no pinned default (or it failed)? -> reopen the last project
    if (!fileStore->getCurrentProjectFile().exists() &&
        ProjectSettingsComponent::readOpenLastProjectEnabled(getPreferences()) &&
        getPreferences().valueExists(PreferenceKeys::lastProjectFile)) {
        const juce::File last(getPreferences().getValue(PreferenceKeys::lastProjectFile));
        if (ProjectFileStore::isValidProjectStructure(last))
            openFile(last);
        else
            getPreferences().removeKey(PreferenceKeys::lastProjectFile); // moved/deleted - forget it silently
    }

    // still nothing loaded? -> create new project
    if (!fileStore->getCurrentProjectFile().exists())
        serializer->createNewProject();
}

void AudiumApplication::offerOrphanedTempProjectRestore()
{
    // crash recovery for never-saved projects: a leftover temp package with an
    // autosave means a previous session died with unsaved work
    const auto orphanedTempProject = ProjectFileStore::findOrphanedTempAutosave();
    if (orphanedTempProject == File())
        return;

    auto options = MessageBoxOptions::makeOptionsYesNo (MessageBoxIconType::QuestionIcon,
                                                        TRANS ("Restore unsaved project?"),
                                                        TRANS ("A previous session ended unexpectedly with an "
                                                               "unsaved project.\n\nDo you want to restore it?"),
                                                        TRANS ("Restore"),
                                                        TRANS ("Discard"));

    NativeMessageBox::showAsync(options, [this, orphanedTempProject] (int result) {
        if (result != 0) {
            orphanedTempProject.deleteRecursively();
            return;
        }

        // the user may have recorded or edited while the dialog was up, and
        // opening replaces the session (and deletes its temp directory) -
        // go through the usual dirty check first
        askToSaveIfDirtyAndInvoke([this, orphanedTempProject] {
            restoreOrphanedTempProject(orphanedTempProject);
        });
    });
}

void AudiumApplication::restoreOrphanedTempProject(juce::File packageDirectory)
{
    // promote the snapshot to a project file (atomically, and only continue
    // on success) and open the temp package like any other project
    const auto autosave = packageDirectory.getChildFile(ProjectFileStore::autosaveFileName);
    juce::TemporaryFile promoted (packageDirectory.getChildFile(ProjectFileStore::projectFileName));

    if (! (autosave.copyFileTo(promoted.getFile()) &&
           promoted.overwriteTargetFileWithTemporary())) {
        NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                              "Error",
                                              "Failed to restore the project from:\n"
                                                  + autosave.getFullPathName());
        return;
    }

    packageDirectory.getChildFile(ProjectFileStore::autosavePidFileName).deleteFile();

    openFile(packageDirectory);

    // offer to save it to a real location - but let the user look at what
    // was recovered first before committing
    auto saveOptions = MessageBoxOptions::makeOptionsYesNo (MessageBoxIconType::InfoIcon,
                                                            TRANS ("Project restored"),
                                                            TRANS ("Check that everything was recovered, "
                                                                   "then choose where to save the project.\n\n"
                                                                   "Until it is saved it remains in a "
                                                                   "temporary location."),
                                                            TRANS ("Save As..."),
                                                            TRANS ("Later"));

    NativeMessageBox::showAsync(saveOptions, [this] (int saveResult) {
        if (saveResult == 0)
            saveProjectAs();
    });
}

void AudiumApplication::startProjectMonitor()
{
    // watch the open package for external (agent) writes and drive the
    // autosave cadence
    projectMonitor = std::make_unique<ProjectMonitor>(audiumEngine);

    projectMonitor->onExternalChange = [this] {
        projectMonitor->setSuspended(true);

        if (fileStore->projectChangedOnDisk()) {
            const auto reloaded = fileStore->reloadFromDisk([](std::string error) {
                std::cout << "external reload failed: " << error << std::endl;
            });

            if (reloaded)
                updateUI(); // includes the window title (dirty + agent markers)
        }
        else {
            // analysis-only write (e.g. `audionaut-cli analyze`): derived
            // data - refresh the cache without dirtying the session or
            // touching the undo history
            fileStore->reloadAnalysisFromDisk();
            updateUI();
        }

        projectMonitor->setSuspended(false);
    };

    projectMonitor->onProjectFileMissing = [this] {
        NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                              TRANS ("Project file missing"),
                                              TRANS ("The project file was moved or deleted on disk:\n")
                                                  + fileStore->getCurrentProjectFile().getFullPathName()
                                                  + TRANS ("\n\nSave the project to recreate it."));
    };

    projectMonitor->onAutosaveDue = [this] {
        // the engine serialises its UI state, so hand it the current view first
        captureUiState();
        fileStore->writeAutosave();
    };
}

void AudiumApplication::askForUsageStatisticsConsent()
{
    if (UsageAnalytics::isConsentDecided (getPreferences()))
        return;

    auto options = MessageBoxOptions::makeOptionsOkCancel (MessageBoxIconType::QuestionIcon,
                                                           TRANS ("Share anonymous usage statistics?"),
                                                           TRANS ("Audionaut can report app launches and feature usage "
                                                                  "to help improve the app. No audio, project content "
                                                                  "or personal data is collected.\n\n"
                                                                  "You can change this anytime in Settings > Privacy."),
                                                           TRANS ("Share"),
                                                           TRANS ("No thanks"));

    NativeMessageBox::showAsync (options, [this] (int result) {
        const auto accepted = result == 0;
        setUsageStatisticsEnabled (accepted);

        if (accepted)
            logAppLaunch (*usageAnalytics);
    });
}

void AudiumApplication::shutdown()
{
    if (isRunningCommandLine)
    {
        // CLI mode created none of the GUI objects below - only preferences
        preferences.reset();
        return;
    }

    logUsageEvent("app_quit");

    getPreferences().setValue(PreferenceKeys::browserWindowOpen, fileBrowserVisible() ? "true" : "false");

    // Add your application's shutdown code here..
    projectMonitor.reset(); // stop polling before the engine goes away
    mainWindow.reset(); // (deletes our window)

    updateSettings();

#if JUCE_MAC
    MenuBarModel::setMacMainMenu (nullptr);
#endif

    menuModel.reset();
    commandManager.reset();

    if (audiumEngine != nullptr)
    {
        audiumEngine->uninitialise();
        audiumEngine.reset();
    }

    // flushes or saves the queued events; must go while the preferences live
    usageAnalytics.reset();
}

void AudiumApplication::askToSaveIfDirtyAndInvoke(std::function<void ()> callback)
{
    jassert(callback != nullptr);
    
    if (!audiumEngine->getUndoManager()->canUndo())
    {
        NullCheckedInvocation::invoke(callback);
    }
    else
    {
        juce::String docName = "Untitled";
        if (fileStore->getCurrentProjectFile().existsAsFile())
            docName = fileStore->getCurrentProjectFile().getFileName();
        

        auto options = MessageBoxOptions::makeOptionsYesNoCancel (MessageBoxIconType::QuestionIcon,
                                                                  TRANS ("Save the changes?"),
                                                                  TRANS ("Do you want to save the changes to \"")
                                                                      + docName + "\"?",
                                                                  TRANS ("Save"),
                                                                  TRANS ("Discard changes"),
                                                                  TRANS ("Cancel"));
        
        // -> std::function<void (int)> callback
        juce::NativeMessageBox::showAsync(options, [this, callback] (int result) {
            if (result == 0) {
                if (fileStore->getCurrentProjectFile().existsAsFile()) {
                    if (saveProject())
                        NullCheckedInvocation::invoke(callback);
                }
                else {
                    if (saveProjectAs())
                        NullCheckedInvocation::invoke(callback);
                }
            }
            else if (result == 1) {
                fileStore->deleteObsoleteAudioFiles();

                // a deliberate discard must not look like a crash on next open
                fileStore->deleteAutosave();

                NullCheckedInvocation::invoke(callback);
            }
        });
    }
}

void AudiumApplication::systemRequestedQuit()
{
    // This is called when the app is being asked to quit: you can ignore this
    // request and let the app carry on running, or call quit() to allow the app to close.
    
    askToSaveIfDirtyAndInvoke([](void) {
        quit();
    });
}

void AudiumApplication::anotherInstanceStarted (const juce::String& commandLine)
{
    if (! commandLine.trim().startsWithChar ('-')) {
        ArgumentList list ({}, commandLine);

        // a forwarded CLI invocation must not have its verb opened as a file
        // (CLI runs normally quit before forwarding; this is insurance)
        if (! list.arguments.isEmpty())
            for (auto& spec : cli::getCliCommands())
                if (list.arguments.getReference (0).text == spec.verb)
                    return;

        for (auto& arg : list.arguments) {
            auto file = arg.resolveAsFile();
            askToSaveIfDirtyAndInvoke([this, file](void) {
                openFile (file);
            
                // will update the open recent menu
                commandManager->commandStatusChanged();
            });
        }
    }
}

void AudiumApplication::initCommandManager()
{
    commandManager = std::make_unique<ApplicationCommandManager>();
    commandManager->registerAllCommandsForTarget (this);
    commandManager->addListener (this);
}

void AudiumApplication::initPreferences()
{
    preferences = std::make_unique<Preferences>();
    getPreferences().init(getApplicationName().toStdString(),
                          getApplicationCompanyName().toStdString());
    
    recentFiles.setMaxNumberOfItems(30);
    if (getPreferences().valueExists(PreferenceKeys::recentFiles)) {
        recentFiles.restoreFromString (getPreferences().getValue (PreferenceKeys::recentFiles));
        recentFiles.removeNonExistentFiles();
    }
}

void AudiumApplication::setUsageStatisticsEnabled(bool enabled)
{
    if (usageAnalytics != nullptr)
        usageAnalytics->setEnabled(enabled);
}

void AudiumApplication::logUsageEvent(const juce::String& name, const juce::StringPairArray& parameters)
{
    if (usageAnalytics != nullptr)
        usageAnalytics->logEvent(name, parameters);
}

void AudiumApplication::applicationCommandInvoked(const ApplicationCommandTarget::InvocationInfo& info)
{
    auto name = commandManager->getNameOfCommand(info.commandID);
    if (name.isEmpty())
        name = "0x" + String::toHexString((int) info.commandID);

    // "Save as..." -> "save_as", so the values work as a GA4 dimension
    StringPairArray params;
    params.set("command", name.toLowerCase()
                              .retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789 _")
                              .trim()
                              .replaceCharacter(' ', '_'));
    logUsageEvent("menu_command", params);
}

void AudiumApplication::applyAnalysisPreferences()
{
    if (auto worker = audiumEngine->getAudioResourceContainer()->getAnalysisWorker()) {
        worker->setAutoAnalysisEnabled (AnalysisSettingsComponent::readAutoAnalysisEnabled (getPreferences()));
        worker->setDefaultAnalysisTypes (AnalysisSettingsComponent::readAutoAnalysisTypes (getPreferences()));
    }
}

MenuBarModel* AudiumApplication::getMenuModel()
{
    return menuModel.get();
}

StringArray AudiumApplication::getMenuNames()
{
    StringArray currentMenuNames { "File", "Edit", "Create", "View"};
    return currentMenuNames;
}

PopupMenu AudiumApplication::createMenu (const String& menuName)
{
    if (menuName == "File")
        return createFileMenu();

    if (menuName == "Edit")
        return createEditMenu();
    
    if (menuName == "Create")
        return createCreateMenu();

    if (menuName == "View")
        return createViewMenu();

    jassertfalse; // names have changed?
    return {};
}

int recentProjectsBaseID = 100;


PopupMenu AudiumApplication::createFileMenu()
{
    PopupMenu menu;
    menu.addCommandItem (commandManager.get(), CommandIDs::newProject);
    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), CommandIDs::openProject);

    {
        PopupMenu recentFilesMenu;
        recentFiles.createPopupMenuItems (recentFilesMenu, recentProjectsBaseID, true, true);
        // std::cout << "recentFiles.createPopupMenuItems" << std::endl;
        if (recentFilesMenu.getNumItems() > 0) {
            recentFilesMenu.addSeparator();
            recentFilesMenu.addCommandItem (commandManager.get(), CommandIDs::clearRecentFiles);
        }

        menu.addSubMenu ("Open Recent", recentFilesMenu);
    }

    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), CommandIDs::saveProject);
    menu.addCommandItem (commandManager.get(), CommandIDs::saveProjectAs);
    menu.addCommandItem (commandManager.get(), CommandIDs::defaultProject);
    menu.addSeparator();
    
    menu.addCommandItem (commandManager.get(), CommandIDs::bounceProject);
    menu.addSeparator();

   #if ! JUCE_MAC
    menu.addCommandItem (commandManager.get(), CommandIDs::showAboutWindow);
    menu.addCommandItem (commandManager.get(), CommandIDs::showSettingsWindow);
//    menu.addCommandItem (commandManager.get(), CommandIDs::checkForNewVersion);
    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::quit);
   #endif

    return menu;
}

PopupMenu AudiumApplication::createEditMenu()
{
    PopupMenu menu;
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::undo);
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::redo);
    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::cut);
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::copy);
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::paste);
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::deselectAll);
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::del);
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::selectAll);
    menu.addSeparator();
    menu.addCommandItem(commandManager.get(), CommandIDs::splitRegion);
    menu.addCommandItem(commandManager.get(), CommandIDs::cleanupRegions);
    menu.addSeparator();

    PopupMenu assembleMenu;
    assembleMenu.addCommandItem(commandManager.get(), CommandIDs::assembleSequential);
    assembleMenu.addCommandItem(commandManager.get(), CommandIDs::assembleRandom);

    PopupMenu autoEditMenu;
    autoEditMenu.addCommandItem(commandManager.get(), CommandIDs::autoEdit);
    autoEditMenu.addSubMenu("2. Assemble", assembleMenu);
    menu.addSubMenu("Auto Edit", autoEditMenu);
    menu.addSeparator();
    menu.addCommandItem(commandManager.get(), CommandIDs::separateStems);

    //menu.addSeparator();
    //menu.addCommandItem(commandManager.get(), CommandIDs::loopPlayList);
    return menu;
}

PopupMenu AudiumApplication::createCreateMenu()
{
    PopupMenu menu;
    menu.addCommandItem (commandManager.get(), CommandIDs::createAudioTrack);
    menu.addSeparator();
    menu.addCommandItem(commandManager.get(), CommandIDs::createRegion);
    return menu;
}

PopupMenu AudiumApplication::createViewMenu()
{
    PopupMenu menu;
    menu.addCommandItem (commandManager.get(), CommandIDs::toggleFullScreen);
    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), CommandIDs::showFileBrowser);
    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), CommandIDs::zoomIn);
    menu.addCommandItem (commandManager.get(), CommandIDs::zoomOut);
    menu.addCommandItem (commandManager.get(), CommandIDs::pageLeft);
    menu.addCommandItem (commandManager.get(), CommandIDs::pageRight);
    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), CommandIDs::followTransport);

    return menu;
}

PopupMenu AudiumApplication::createExtraAppleMenuItems()
{
    PopupMenu menu;
    menu.addCommandItem (commandManager.get(), CommandIDs::showAboutWindow);
    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), CommandIDs::showSettingsWindow);
    return menu;
}


void AudiumApplication::handleMainMenuCommand (int menuItemID)
{
    if (menuItemID >= recentProjectsBaseID && menuItemID < (recentProjectsBaseID + 100)) {
        askToSaveIfDirtyAndInvoke([this, menuItemID](void) {
            openFile (recentFiles.getFile (menuItemID - recentProjectsBaseID));
        
            // will update the open recent menu
            commandManager->commandStatusChanged();
            
        });
    }
}

void AudiumApplication::getAllCommands (Array <CommandID>& commands)
{
    JUCEApplication::getAllCommands (commands);

    const CommandID ids[] = {   CommandIDs::newProject,
                                CommandIDs::openProject,
                                CommandIDs::defaultProject,
                                CommandIDs::saveProject,
                                CommandIDs::saveProjectAs,
                                CommandIDs::showAboutWindow,
                                CommandIDs::showSettingsWindow,
                                CommandIDs::clearRecentFiles,
                                CommandIDs::showFileBrowser,
    };

    commands.addArray (ids, numElementsInArray (ids));
}

void AudiumApplication::getCommandInfo (CommandID commandID, ApplicationCommandInfo& result)
{
    switch (commandID)
    {
    case CommandIDs::newProject:
        result.setInfo ("New Project...", "Creates a new project", CommandCategories::general, 0);
        result.defaultKeypresses.add (KeyPress ('n', ModifierKeys::commandModifier, 0));
        break;

    case CommandIDs::openProject:
        result.setInfo ("Open...", "Opens a project", CommandCategories::general, 0);
        result.defaultKeypresses.add (KeyPress ('o', ModifierKeys::commandModifier, 0));
        break;
            
    case CommandIDs::defaultProject:
        result.setInfo ("Set current project as default", "Set current project as default", CommandCategories::general, 0);
        break;
            
    case CommandIDs::saveProject:
        result.setInfo ("Save", "Saves a project", CommandCategories::general, 0);
        result.defaultKeypresses.add (KeyPress ('s', ModifierKeys::commandModifier, 0));
        break;

    case CommandIDs::saveProjectAs:
        result.setInfo ("Save as...", "Saves the current project to a new location", CommandCategories::general, 0);
        result.defaultKeypresses.add ({ 's', ModifierKeys::commandModifier | ModifierKeys::shiftModifier, 0 });
        break;
    case CommandIDs::showAboutWindow:
        result.setInfo ("About", "Shows the 'About' page.", CommandCategories::general, 0);
        break;
    case CommandIDs::showSettingsWindow:
        result.setInfo ("Settings...", "Shows the 'Settings' dialog.", CommandCategories::general, 0);
        result.defaultKeypresses.add (KeyPress (',', ModifierKeys::commandModifier, 0));
        break;
            
    case CommandIDs::checkForNewVersion:
        result.setInfo ("Check for New Version...", "Checks the web server for a new version", CommandCategories::general, 0);
        break;
    case CommandIDs::clearRecentFiles:
        result.setInfo ("Clear Recent Files", "Clears all recent files from the menu", CommandCategories::general, 0);
        result.setActive (recentFiles.getNumFiles() > 0);
        break;
    case CommandIDs::showFileBrowser:
        result.setInfo ("Open File Browser", "Open File Browser window.", CommandCategories::view, 0);
        result.defaultKeypresses.add (KeyPress (KeyPress::numberPad5, ModifierKeys::shiftModifier, 0));
        break;

    default:
        JUCEApplication::getCommandInfo (commandID, result);
        break;
    }
}

bool AudiumApplication::perform (const InvocationInfo& info)
{
    switch (info.commandID)
    {
        case CommandIDs::newProject:
            createNewProject();
            break;
        case CommandIDs::openProject:
            askUserToOpenFile();
            break;
        case CommandIDs::defaultProject:
            if (fileStore->getCurrentProjectFile() != File()) {
                auto projectFile = fileStore->getCurrentProjectFile();
                if (!audium::ProjectFileStore::isValidProjectStructure(projectFile)) {
                    // projectFile.audium/Project.json -> ../
                    projectFile = projectFile.getParentDirectory();
                }
                jassert(audium::ProjectFileStore::isValidProjectStructure(projectFile));
                getPreferences().setValue(PreferenceKeys::defaultFile, projectFile.getFullPathName().toStdString());
            }
            else {
                getPreferences().removeKey(PreferenceKeys::defaultFile);
            }
            break;
        case CommandIDs::saveProject:
            if (fileStore->getCurrentProjectFile() == File()) {
                saveProjectAs();
            }
            else {
                saveProject();
            }
            break;
        case CommandIDs::saveProjectAs:
            saveProjectAs();
            break;
        case CommandIDs::showAboutWindow:
            showAboutWindow();
            break;
        case CommandIDs::showSettingsWindow:
            showSettingsDialog();
            break;
        case CommandIDs::clearRecentFiles:
            clearRecentFiles();
            break;
        case CommandIDs::showFileBrowser:
            showFileBrowserWindow();
            break;

        default:
            return JUCEApplication::perform (info);
    }

    return true;
}


#if JUCE_MAC
 void AudiumApplication::rebuildAppleMenu()
 {
     auto extraAppleMenuItems = createExtraAppleMenuItems();

     // workaround broken "Open Recent" submenu: not passing the
     // submenu's title here avoids the defect in JuceMainMenuHandler::addMenuItem
     MenuBarModel::setMacMainMenu (menuModel.get(), &extraAppleMenuItems); //, "Open Recent");
 }
#endif


void AudiumApplication::createNewProject()
{
    askToSaveIfDirtyAndInvoke([this](void) {
        serializer->cleanup();
        serializer->createNewProject();
        updateUI();

        // cleanup() cleared the ui state, so this resets the view to its defaults
        restoreUiState();

        refreshWindowTitle();
    });
}

void AudiumApplication::askUserToOpenFile()
{
    askToSaveIfDirtyAndInvoke([this](void) {
    
        // open chooser...
        
        auto wildcard = "*" + String(audium::ProjectFileStore::projectFileExtension) + ";";
        wildcard += "*.json;";
        wildcard += audiumEngine->getAudioResourceContainer()->getAudioFormatManager()->getWildcardForAllFormats();
        chooser = std::make_unique<FileChooser> ("Open File", initialOpenDirectory, wildcard);
        auto flags = FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectDirectories;

        chooser->launchAsync (flags, [this] (const FileChooser& fc) {
            openFile(fc.getResult());
        });
        
    });
}

void AudiumApplication::openFile(juce::File file)
{
    // crash recovery: an Autosave.json newer than Project.json means the app
    // previously quit without a clean save. The restore is OFFERED only after
    // the project itself opened successfully (openFileInternal) - openFile
    // stays synchronous for its callers, and a corrupt Project.json can never
    // take the snapshot down with it.
    juce::File packageDirectory;
    if (audium::ProjectFileStore::isValidProjectStructure(file))
        packageDirectory = file;
    else if (file.getFileName() == audium::ProjectFileStore::projectFileName)
        packageDirectory = file.getParentDirectory();

    juce::File autosaveToOffer;
    if (packageDirectory != File()) {
        const auto autosave = packageDirectory.getChildFile(audium::ProjectFileStore::autosaveFileName);
        const auto projectJson = packageDirectory.getChildFile(audium::ProjectFileStore::projectFileName);

        if (autosave.existsAsFile() &&
            autosave.getLastModificationTime() > projectJson.getLastModificationTime())
            autosaveToOffer = autosave;
    }

    openFileInternal(file, autosaveToOffer);
}

void AudiumApplication::openFileInternal(juce::File file, juce::File autosaveToOffer)
{
    // audio files are added to the current project - only a project file brings its own view state
    const auto isProjectFile = audium::ProjectFileStore::isValidProjectStructure(file) ||
                               audium::ProjectFileStore::isJsonProjectFile(file);

    auto success = fileStore->open(file, [this, file](std::string error) {
        NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                              "Error",
                                              "Failed to open:\n" + file.getFullPathName() + "\n\n" + String(error));

    });

    if (success) {

        initialOpenDirectory = file.getParentDirectory();
        std::cout << "initialOpenDirectory = " << initialOpenDirectory.getFullPathName() << std::endl;
        RecentlyOpenedFilesList::registerRecentFileNatively (file);
        recentFiles.addFile (file);
        if (isProjectFile)
            rememberLastProject (file);
        updateSettings();

        logUsageEvent(isProjectFile ? "project_open" : "file_import");
    }

    updateUI();

    // the view state is only meaningful once the tracks it refers to are laid out.
    // a failed load leaves the engine on a fresh project with an empty ui state,
    // which resets the view to its defaults - that is what we want in that case too.
    if (isProjectFile)
        restoreUiState();

    refreshWindowTitle();

    // offer the crash-recovery snapshot now that the project is open. Restore
    // applies it as an undoable step: the session starts dirty and Undo
    // returns to the saved state; the snapshot stays until the next clean
    // save. Discard only ever runs after a successful open.
    if (success && autosaveToOffer != File()) {
        auto options = MessageBoxOptions::makeOptionsYesNo (MessageBoxIconType::QuestionIcon,
                                                            TRANS ("Restore unsaved changes?"),
                                                            "\"" + autosaveToOffer.getParentDirectory().getFileName() + "\" "
                                                                + TRANS ("has unsaved changes from a previous session "
                                                                         "(the app may have quit unexpectedly).\n\n"
                                                                         "Do you want to restore them?"),
                                                            TRANS ("Restore"),
                                                            TRANS ("Discard"));

        NativeMessageBox::showAsync(options, [this, autosaveToOffer] (int result) {
            if (result == 0) {
                const auto restored = fileStore->restoreAutosave([this](std::string error) {
                    NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                          "Error",
                                                          "Failed to restore unsaved changes.\n\n" + String(error));
                });

                if (restored) {
                    updateUI();
                    restoreUiState();
                    refreshWindowTitle();
                }
            }
            else {
                autosaveToOffer.deleteFile();
            }
        });
    }
}

const File createProjectDirectory(const File &inFile)
{
    auto projectDir =   inFile.getParentDirectory().getFullPathName() +
                        File::getSeparatorString() +
                        inFile.getFileNameWithoutExtension() +
                        audium::ProjectFileStore::projectFileExtension;
    
    if (!File(projectDir).exists()) {
        File(projectDir).createDirectory();
    }
    return File(projectDir + File::getSeparatorString() + audium::ProjectFileStore::projectFileName);
}

juce::MessageBoxOptions getAskToOverwriteFileOptions (const File& newFile)
{
    return MessageBoxOptions::makeOptionsOkCancel (MessageBoxIconType::WarningIcon,
                                                   TRANS ("File already exists"),
                                                   TRANS ("There's already a file called: FLNM")
                                                           .replace ("FLNM", newFile.getFullPathName())
                                                       + "\n\n"
                                                       + TRANS ("Are you sure you want to overwrite it?"),
                                                   TRANS ("Overwrite"),
                                                   TRANS ("Cancel"));
}

bool askToOverwriteFileSync (const File& newFile)
{
    auto result = NativeMessageBox::show (getAskToOverwriteFileOptions (newFile));
    return (result == 0);
}


bool AudiumApplication::saveProjectAs()
{
    chooser = std::make_unique<FileChooser> (("Save As..."),
                                             initialSaveDirectory);
    auto flags =    FileBrowserComponent::saveMode |
                    FileBrowserComponent::canSelectFiles |
                    FileBrowserComponent::canSelectDirectories |
                    FileBrowserComponent::warnAboutOverwriting;

    chooser->launchAsync (flags, [this] (const FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != File()) {
            
            // warn overwrite: special case where user selects an existing project (without the .audium extension)
            auto existingProject = File(file.getFullPathName() + audium::ProjectFileStore::projectFileExtension);
            if (existingProject.exists()) {
                if (!askToOverwriteFileSync(existingProject))
                    return false;
            }
            
            if (file.existsAsFile())
                return saveProjectToFile(file);
            else
                return saveProjectToFile(createProjectDirectory(file));
        }
        return false;
    });
    
    return false;
}

bool AudiumApplication::saveProjectToFile(juce::File file)
{
    // the engine serialises its UI state, so hand it the current view first
    captureUiState();

    auto success = fileStore->save(file, [this, file] (std::string error) {

        NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                              "Error",
                                              "Failed to save " + file.getFullPathName() +"\n\n" + String(error));
    });
    
    if (success) {
        // foo.audium/Project.json -> ../../
        initialSaveDirectory = file.getParentDirectory().getParentDirectory();
        std::cout << "file saved: " << file.getFullPathName() << std::endl;
        fileStore->deleteObsoleteAudioFiles();
        std::cout << "initialSaveDirectory: " << initialSaveDirectory.getFullPathName() << std::endl;
        
        if (file.getFileName() == audium::ProjectFileStore::projectFileName)
            file = file.getParentDirectory();
        
        if (file.getFileName().contains(audium::ProjectFileStore::projectFileExtension)) {
            // only save .audium in recent list
            RecentlyOpenedFilesList::registerRecentFileNatively (file);
            recentFiles.addFile (file);
            rememberLastProject (file);
        }
        updateSettings();

        logUsageEvent("project_save");

        refreshWindowTitle();
    }
    return success;
}

bool AudiumApplication::saveProject()
{
    return saveProjectToFile(fileStore->getCurrentProjectFile());
}

// a file passed on the command line is opened before the main window exists
MainComponent* AudiumApplication::getMainComponent() const
{
    if (mainWindow == nullptr)
        return nullptr;

    return dynamic_cast<MainComponent*>(mainWindow->getContentComponent());
}

void AudiumApplication::updateUI()
{
    if (auto comp = getMainComponent()) {
        comp->rebuildUI();
        comp->updateUI();
    }
}

void AudiumApplication::refreshWindowTitle()
{
    // MainComponent::updateWindowTitle owns the title format - it derives
    // everything (project name, dirty marker, agent marker) from the engine
    if (auto comp = getMainComponent())
        comp->updateWindowTitle();
}

void AudiumApplication::captureUiState()
{
    if (auto comp = getMainComponent())
        comp->writeUiState(serializer->getUiState());
}

void AudiumApplication::restoreUiState()
{
    if (auto comp = getMainComponent())
        comp->readUiState(serializer->getUiState());
}

void AudiumApplication::showAboutWindow()
{
    if (aboutComponent != nullptr)
    {
        aboutComponent->toFront (true);
        return;
    }

    // The credits list varies with the backends compiled in, so the
    // component decides how tall the window has to be.
    auto* about = new AboutWindowComponent();
    auto w = 600;
    auto h = about->getPreferredHeight();
    new FloatingToolWindow ("About", "AboutWindowPosition",
                            about,
                            aboutComponent, false,
                            w, h, w, h, w, h);
}

void AudiumApplication::showFileBrowserWindow()
{
    auto w = 300;
    auto h = 800;
    if (fileBrowserView != nullptr)
        fileBrowserView->toFront (true);
    else
        new FloatingToolWindow ("File Browser", audium::PreferenceKeys::browserWindowState,
                                new FileBrowserView(),
                                fileBrowserView, true,
                                w, h, 50, 50, 3000, 3000);
}

void AudiumApplication::showSettingsDialog()
{
    if (settingsDialog == nullptr)
        settingsDialog = std::make_shared<SettingsDialog>(audiumEngine);
    settingsDialog->invoke(mainWindow->getContentComponent());
}

void AudiumApplication::clearRecentFiles()
{
    recentFiles.clear();
    recentFiles.clearRecentFilesNatively();
    updateSettings();
    menuModel->menuItemsChanged();
}

void AudiumApplication::rememberLastProject(juce::File projectPackage)
{
    if (projectPackage.getFileName() == audium::ProjectFileStore::projectFileName)
        projectPackage = projectPackage.getParentDirectory();

    getPreferences().setValue(PreferenceKeys::lastProjectFile, projectPackage.getFullPathName().toStdString());
}

void AudiumApplication::updateSettings()
{
    getPreferences().setValue(PreferenceKeys::initialOpenDirectory, initialOpenDirectory.getFullPathName().toStdString());
    getPreferences().setValue(PreferenceKeys::initialSaveDirectory, initialSaveDirectory.getFullPathName().toStdString());
    getPreferences().setValue(PreferenceKeys::recentFiles, recentFiles.toString().toStdString());
    getPreferences().synchronize();
}

bool AudiumApplication::fileBrowserVisible() const
{
    if (fileBrowserView != nullptr &&
        fileBrowserView->isVisible()) {
        return true;
    }
    return false;
}
