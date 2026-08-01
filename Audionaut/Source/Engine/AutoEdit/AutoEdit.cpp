//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <JuceHeader.h>

#include "AutoEdit.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/ResourceGroup.h"
#include "Engine/Undo/UndoableContainerAction.h"

namespace audium {

namespace {

// The audio the analyses describe, and that the regions are created against.
struct EditTarget {
    std::shared_ptr<AudioTrack> track;
    std::shared_ptr<ResourceGroup> resourceGroup;
    std::shared_ptr<AudioResource> resource;

    bool isValid() const
    {
        return track != nullptr && resourceGroup != nullptr && resource != nullptr;
    }
};

/**
 * Works out which audio an edit applies to.
 *
 * The arrangement is what the user is editing, so the playlist item chosen in
 * the dialog picks the target: its region names the resource group, and that
 * group names the audio file. With the usual one item to one region to one file
 * arrangement this is the same audio the fallback would find, but it stops
 * being so as soon as a track carries more than one.
 */
EditTarget resolveEditTarget(std::shared_ptr<AudioTrack> track, int playListItemId)
{
    EditTarget target;
    target.track = track;

    if (track == nullptr)
        return target;

    if (auto playListContainer = track->getPlayListContainer())
        if (auto item = playListContainer->getPlayListItem(playListItemId))
            if (auto region = item->getRegion())
                target.resourceGroup = region->getResourceGroup();

    // No playlist item named, or one that resolves to nothing: fall back to the
    // track's first group, which is where a single-file track keeps its audio.
    if (target.resourceGroup == nullptr)
    {
        auto resourceGroups = track->getResourceGroups();

        if (! resourceGroups.empty())
            target.resourceGroup = resourceGroups[0];
    }

    if (target.resourceGroup != nullptr)
    {
        auto resources = target.resourceGroup->getAudioResources();

        if (! resources.empty())
            target.resource = resources[0];
    }

    return target;
}

// Names the missing analyses so the user is told what to wait for rather than
// just that nothing happened.
std::string describeMissing(const std::vector<AnalysisType>& missing)
{
    std::string names;

    for (const auto& analysisType : missing)
    {
        if (! names.empty())
            names += ", ";

        names += analysisTypeToString(analysisType);
    }

    return names;
}

} // namespace

const juce::String AutoEdit::getTempDirectory()
{
    // Temp directory on is ~Library/Caches/AppAudium
    return juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName();
}

bool AutoEdit::invokeAutoEdit(AutoEditConfig &config,
                              std::function<void(std::string)> callback)
{
    auto audioTrackContainer = audiumEngine->getAudioTrackContainer();

    auto track = audioTrackContainer->getAudioTrack(config.trackId);

    if (track == nullptr)
    {
        NullCheckedInvocation::invoke(callback, "Please select a valid track to edit.");
        return false;
    }

    // The arrangement is what is being edited, so the chosen playlist item
    // decides which audio that is.
    const auto target = resolveEditTarget(track, config.playlistItemId);

    if (! target.isValid())
    {
        NullCheckedInvocation::invoke(callback, "The selected clip has no audio to edit.");
        return false;
    }

    auto resourceGroup = target.resourceGroup;
    auto resource = target.resource;
    const auto audioFile = juce::File(resource->getFullPathName());

    if (! audioFile.existsAsFile())
    {
        NullCheckedInvocation::invoke(callback, "The track's audio file could not be found.");
        return false;
    }

    audioResourceFilePath = audioFile.getFullPathName().toStdString();

    if (config.source == AutoEditConfig::Source::Python)
        return invokePython(audioFile, config, std::move(callback));

    auto analysisProvider = audioTrackContainer->getAnalysisProvider();

    if (analysisProvider == nullptr)
    {
        NullCheckedInvocation::invoke(callback, "Analysis is unavailable.");
        return false;
    }

    // Cache-only: a partial set would give plausible but different cuts, so the
    // user is told what is still running instead.
    auto missing = analysisProvider->findMissingMergeAnalyses(audioFile);

    if (! missing.empty())
    {
        NullCheckedInvocation::invoke(callback,
                                      "Still analysing " + audioFile.getFileName().toStdString()
                                          + " (" + describeMissing(missing) + "). Please try again shortly.");
        return false;
    }

    const auto duration = resource->getFileLength(audium::seconds);

    if (duration <= 0.0)
    {
        NullCheckedInvocation::invoke(callback, "The track's audio file appears to be empty.");
        return false;
    }

    EventMerger::Parameters params;
    params.numSegments = config.numSegments;

    auto result = analysisProvider->mergeCachedAnalyses(audioFile, (float) duration, params);

    // One boundary bounds no region - two are needed before anything can be cut.
    if (result.boundaries.size() < 2)
    {
        NullCheckedInvocation::invoke(callback, "No segment boundaries were found in this audio.");
        return false;
    }

    const auto boundaries = result.boundaries;

    return applyAsUndoableEdit([this, &boundaries, track, resourceGroup]
    {
        return createRegionsFromBoundaries(boundaries, track, resourceGroup);
    });
}

bool AutoEdit::applyAsUndoableEdit(std::function<bool()> createRegions)
{
    auto audioTrackContainer = audiumEngine->getAudioTrackContainer();
    auto action = std::make_unique<audium::UndoableContainerAction>(*audioTrackContainer.get());

    if (! createRegions())
        return false;

    // Undo: store new state, so a single Undo removes every region this edit
    // created.
    action->storeNewState();
    audioTrackContainer->getUndoManager()->perform(action.release(), "Auto Edit");
    audioTrackContainer->getUndoManager()->beginNewTransaction();

    return true;
}

bool AutoEdit::createRegionsFromBoundaries(const std::vector<float>& boundarySeconds,
                                           std::shared_ptr<AudioTrack> track,
                                           std::shared_ptr<ResourceGroup> resourceGroup)
{
    if (track == nullptr || resourceGroup == nullptr || boundarySeconds.size() < 2)
        return false;

    auto regionContainer = resourceGroup->getAudioRegionContainer();

    if (regionContainer == nullptr)
        return false;

    int counter = 1;

    // Consecutive boundaries bound one region each, so n boundaries give n - 1
    // regions that tile the material without gaps.
    for (size_t i = 1; i < boundarySeconds.size(); ++i)
    {
        juce::Range<double> position;
        position.setStart((double) boundarySeconds[i - 1]);
        position.setEnd((double) boundarySeconds[i]);

        juce::String regionName = "seg-" + juce::String(counter++);

        regionContainer->createRegion(regionName,
                                      position,
                                      track,
                                      resourceGroup,
                                      nullptr,
                                      audium::seconds);
    }

    return counter > 1;
}

bool AutoEdit::invokePython(const juce::File& audioFile,
                            AutoEditConfig &config,
                            std::function<void(std::string)> callback)
{
    // NOTE: Make sure PATH and PYTHONPATH is set correctly.
    // With XCode you must edit the scheme and set the environment variables
    // double check with:
    // system("env");

    // Path to python binary (/opt/homebrew/bin/)
    std::string python = "python3";

    const auto audioFilePath = audioFile.getFullPathName().toStdString();

    // Build the command line string
    std::string commandString;
    commandString += "cd " + getTempDirectory().toStdString() + ";";
    // --verbose
    commandString += python + " $HOME/dev/gaborgandalf/gaborgandalf/automain.py autoedit";
    commandString += " --duration " + std::to_string(config.duration);
    commandString += " --numsegs " + std::to_string(config.numSegments);
    commandString += " --filenames " + audioFilePath;

    try
    {
        if (std::system(commandString.c_str()) != 0)
        {
            NullCheckedInvocation::invoke(callback, "The autoedit script failed. Check that gaborgandalf is installed and its Python environment works.");
            return false;
        }
    }
    catch (std::exception &e)
    {
        NullCheckedInvocation::invoke(callback, e.what());
        return false;
    }

    // The script writes its segment positions in samples of the file it was
    // given, so they are converted with that file's rate - not a fixed one.
    auto audioTrackContainer = audiumEngine->getAudioTrackContainer();
    const auto target = resolveEditTarget(audioTrackContainer->getAudioTrack(config.trackId),
                                          config.playlistItemId);

    if (! target.isValid())
        return false;

    const auto sampleRate = target.resource->getSampleRate();

    if (sampleRate <= 0.0)
    {
        NullCheckedInvocation::invoke(callback, "The track's audio file has no usable sample rate.");
        return false;
    }

    std::string segFileName = getTempDirectory().toStdString() + "/data/segs/"
                                  + getBaseName() + "-seg-data.json";

    return applyAsUndoableEdit([this, segFileName, sampleRate, target]
    {
        return createRegionsFromSegFile(segFileName, sampleRate, target.track, target.resourceGroup);
    });
}

const std::string AutoEdit::getBaseName() const
{
    return juce::File(audioResourceFilePath).getFileNameWithoutExtension().toStdString();
}

bool AutoEdit::createRegionsFromSegFile(std::string segFileName,
                                       double sampleRate,
                                       std::shared_ptr<AudioTrack> track,
                                       std::shared_ptr<ResourceGroup> resourceGroup)
{
    if (track == nullptr || resourceGroup == nullptr)
        return false;

    std::fstream segFile;
    segFile.open(segFileName, std::ios::in);

    if (! segFile.is_open())
    {
        std::cout << "error seg file not found: " << segFileName << std::endl;
        return false;
    }

    int counter = 1;
    auto segdata = nlohmann::json::parse(segFile);

    // create regions from parsed result
    for (auto& elem : segdata)
    {
        juce::Range<double> position;
        position.setStart(static_cast<double>(elem["start"]) / sampleRate);
        position.setEnd(static_cast<double>(elem["end"]) / sampleRate);
        juce::String regionName = "seg-" + juce::String(counter++);

        resourceGroup->getAudioRegionContainer()->createRegion(regionName,
                                                               position,
                                                               track,
                                                               resourceGroup,
                                                               nullptr,
                                                               audium::seconds);
    }

    segFile.close();
    return counter > 1;
}

} // namespace audium
