//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ProjectFileStore.h"

#if !JUCE_WINDOWS
 #include <unistd.h>
 #include <signal.h>
 #include <cerrno>
#endif

namespace audium {

const char* ProjectFileStore::projectFileExtension = ".audium";
const char* ProjectFileStore::projectFileName = "Project.json";
const char* ProjectFileStore::autosaveFileName = "Autosave.json";
const char* ProjectFileStore::autosavePidFileName = "Autosave.pid";

static int getCurrentProcessId()
{
#if JUCE_WINDOWS
    return (int) GetCurrentProcessId();
#else
    return (int) getpid();
#endif
}

static bool isProcessAlive (int pid)
{
    if (pid <= 0)
        return false;
#if JUCE_WINDOWS
    if (auto* handle = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD) pid)) {
        DWORD exitCode = 0;
        const auto running = GetExitCodeProcess (handle, &exitCode) && exitCode == STILL_ACTIVE;
        CloseHandle (handle);
        return running;
    }
    return false;
#else
    return ::kill ((pid_t) pid, 0) == 0 || errno == EPERM;
#endif
}

bool ProjectFileStore::isJsonProjectFile (const juce::File& file)
{
    return (file.existsAsFile() &&
            (file.hasFileExtension(projectFileExtension) ||
             file.hasFileExtension(".json")));
}

bool ProjectFileStore::isValidProjectStructure (const juce::File& file)
{
    return (file.isDirectory() &&
            file.getFileName().endsWith(projectFileExtension) &&
            file.getChildFile(projectFileName).existsAsFile());
}

json ProjectFileStore::readProjectJson (const juce::File& file)
{
    juce::FileInputStream inputStream (file);
    if (!inputStream.openedOk())
        throw std::runtime_error(inputStream.getStatus().getErrorMessage().toStdString());

    auto inputString = inputStream.readString().toStdString();
    if (inputString.empty())
        throw std::runtime_error("empty project file");

    return json::parse(inputString);
}

bool ProjectFileStore::writeJsonAtomically (const juce::File& target, const json& content, std::string& error)
{
    juce::TemporaryFile temp (target);
    auto success = false;
    if (auto out = std::unique_ptr<juce::FileOutputStream>(temp.getFile().createOutputStream())) {
        if (out->failedToOpen()) {
            error = out->getStatus().getErrorMessage().toStdString();
            return false;
        }

        // same framing as Streamable::writeToStream
        out->writeString(content.dump(2));
        out->flush();
        success = true;
    }

    return success && temp.overwriteTargetFileWithTemporary();
}

void ProjectFileStore::refreshStamps (const juce::File& projectFile, const juce::File& analysisFile)
{
    projectFileStamp = projectFile.getLastModificationTime();
    analysisFileStamp = analysisFile.getLastModificationTime();
}

void ProjectFileStore::setStamps (juce::Time projectMtime, juce::Time analysisMtime)
{
    projectFileStamp = projectMtime;
    analysisFileStamp = analysisMtime;
}

void ProjectFileStore::setAnalysisStamp (juce::Time analysisMtime)
{
    analysisFileStamp = analysisMtime;
}

bool ProjectFileStore::projectChangedOnDisk (const juce::File& projectFile) const
{
    return projectFile.getLastModificationTime() != projectFileStamp;
}

bool ProjectFileStore::analysisChangedOnDisk (const juce::File& analysisFile) const
{
    return analysisFile.getLastModificationTime() != analysisFileStamp;
}

void ProjectFileStore::writePidGuardIfTemporary (const juce::File& packageDirectory)
{
    const auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory);
    if (packageDirectory == tempRoot || packageDirectory.isAChildOf(tempRoot))
        packageDirectory.getChildFile(autosavePidFileName)
                        .replaceWithText(juce::String(getCurrentProcessId()));
}

void ProjectFileStore::deleteAutosaveIn (const juce::File& packageDirectory)
{
    if (packageDirectory != juce::File()) {
        packageDirectory.getChildFile(autosaveFileName).deleteFile();
        packageDirectory.getChildFile(autosavePidFileName).deleteFile();
    }
}

juce::File ProjectFileStore::findOrphanedTempAutosave (const juce::File& currentTempDirectory)
{
    juce::File newest;
    juce::Time newestTime;

    const auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto pattern = "temp-*" + juce::String(projectFileExtension);

    for (const auto& dir : tempRoot.findChildFiles(juce::File::findDirectories, false, pattern)) {

        if (dir == currentTempDirectory)
            continue; // this session's own temp directory

        const auto autosave = dir.getChildFile(autosaveFileName);
        if (!autosave.existsAsFile())
            continue;

        // a directory whose Project.json is current was already restored
        const auto projectJson = dir.getChildFile(projectFileName);
        if (projectJson.existsAsFile() &&
            projectJson.getLastModificationTime() >= autosave.getLastModificationTime())
            continue;

        // skip snapshots owned by another live instance
        const auto pidFile = dir.getChildFile(autosavePidFileName);
        if (pidFile.existsAsFile() && isProcessAlive(pidFile.loadFileAsString().getIntValue()))
            continue;

        if (newest == juce::File() || autosave.getLastModificationTime() > newestTime) {
            newestTime = autosave.getLastModificationTime();
            newest = dir;
        }
    }

    return newest;
}

} // namespace audium
