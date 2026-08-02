//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <cstdlib>
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

    // The stretch of audio the chosen clip covers, in seconds. Empty when no
    // playlist item named one, in which case the caller substitutes the whole
    // file.
    juce::Range<double> extent;

    // Index of the clip the extent came from, or -1 when none was resolved and
    // the whole file is being used.
    int playListItemIndex = -1;

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
    {
        auto item = playListContainer->getPlayListItem(playListItemId);

        // The dialog sends -1 when its clip list is empty - it hides clips
        // under a second - and an id that no longer resolves is possible too.
        // Editing the whole file in that case would silently ignore where the
        // clip starts, so the track's first clip is used instead.
        if (item == nullptr)
            item = playListContainer->getPlayListItem(0);

        if (item != nullptr)
            if (auto region = item->getRegion())
            {
                target.resourceGroup = region->getResourceGroup();

                // A clip need not span its whole file, and only what it covers
                // is being edited.
                target.extent = region->getRegionData(audium::seconds);
                target.playListItemIndex = playListContainer->getPlayListItemIndex(item.get());
            }
    }

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

/**
 * Finds a Python interpreter that can actually run gaborgandalf.
 *
 * A plain `python3` is no use: launched from Finder the app inherits a minimal
 * PATH and resolves it to Apple's /usr/bin/python3, and from a shell it finds
 * whatever Homebrew has installed - neither carrying essentia and librosa. So
 * candidates are probed in order and the first one able to import the modules
 * the script needs wins.
 *
 * Set AUDIONAUT_PYTHON to override the search entirely.
 *
 * @return The interpreter path, or an empty string when none can run the script.
 */
std::string findPythonInterpreter()
{
    std::vector<std::string> candidates;

    if (auto* override_ = std::getenv("AUDIONAUT_PYTHON"))
        candidates.emplace_back(override_);

    const auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                          .getFullPathName().toStdString();

    candidates.push_back(home + "/.venvs/gaborgandalf/bin/python3");
    candidates.push_back("/opt/homebrew/bin/python3");
    candidates.push_back("/usr/local/bin/python3");
    // Last resort: whatever PATH offers, which is what this used to assume.
    candidates.push_back("python3");

    for (const auto& candidate : candidates)
    {
        // Quiet on both streams: this runs several times and a failing probe is
        // expected, not an error worth printing.
        const auto probe = "\"" + candidate
                               + "\" -c \"import essentia, librosa, scipy.signal; "
                                 "assert hasattr(scipy.signal, 'ricker')\" >/dev/null 2>&1";

        if (std::system(probe.c_str()) == 0)
            return candidate;
    }

    return {};
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

    // The analyses describe the whole file; the edit applies only to what the
    // chosen clip covers. Without a clip to go on, that is the whole file.
    const auto extent = target.extent.isEmpty() ? juce::Range<double>(0.0, duration)
                                                : target.extent;

    const auto replaceClip = config.replacePlayListItem;
    const auto clipIndex = target.playListItemIndex;

    if (! applyAsUndoableEdit([this, &boundaries, extent, track, resourceGroup,
                               replaceClip, clipIndex]
        {
            auto created = createRegionsFromBoundaries(boundaries, extent, track, resourceGroup);

            if (created.empty())
                return false;

            // Inside the same transaction, so one Undo takes the arrangement
            // back along with the regions.
            if (replaceClip)
                replacePlayListItemWithRegions(track, clipIndex, created);

            return true;
        }))
    {
        NullCheckedInvocation::invoke(callback,
                                      "No segment boundaries fall inside the selected clip.");
        return false;
    }

    return true;
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

std::vector<std::shared_ptr<AudioRegion>>
    AutoEdit::createRegionsFromBoundaries(const std::vector<float>& boundarySeconds,
                                          juce::Range<double> extent,
                                          std::shared_ptr<AudioTrack> track,
                                          std::shared_ptr<ResourceGroup> resourceGroup)
{
    std::vector<std::shared_ptr<AudioRegion>> created;

    if (track == nullptr || resourceGroup == nullptr || extent.getLength() <= 0.0)
        return created;

    auto regionContainer = resourceGroup->getAudioRegionContainer();

    if (regionContainer == nullptr)
        return created;

    // Boundaries describe the whole file, so those outside the clip are of no
    // use here. The ones that remain are bracketed by the clip's own edges, so
    // the regions tile exactly what was selected and nothing beyond it.
    std::vector<double> points { extent.getStart() };

    for (auto boundary : boundarySeconds)
    {
        const auto point = (double) boundary;

        if (point > extent.getStart() && point < extent.getEnd())
            points.push_back(point);
    }

    points.push_back(extent.getEnd());

    // Nothing fell inside, so there is no cut to make - one region spanning the
    // clip would just restate what is already there.
    if (points.size() < 3)
        return created;

    int counter = 1;

    for (size_t i = 1; i < points.size(); ++i)
    {
        juce::Range<double> position;
        position.setStart(points[i - 1]);
        position.setEnd(points[i]);

        juce::String regionName = "seg-" + juce::String(counter++);

        if (auto region = regionContainer->createRegion(regionName,
                                                        position,
                                                        track,
                                                        resourceGroup,
                                                        nullptr,
                                                        audium::seconds))
            created.push_back(region);
    }

    return created;
}

bool AutoEdit::replacePlayListItemWithRegions(std::shared_ptr<AudioTrack> track,
                                              int playListItemIndex,
                                              const std::vector<std::shared_ptr<AudioRegion>>& regions)
{
    if (track == nullptr || regions.empty())
        return false;

    auto playListContainer = track->getPlayListContainer();

    if (playListContainer == nullptr)
        return false;

    auto original = playListContainer->getPlayListItem(playListItemIndex);

    if (original == nullptr)
        original = playListContainer->getPlayListItem(0);

    if (original == nullptr)
        return false;

    const auto insertAt = playListContainer->getPlayListItemIndex(original.get());

    if (insertAt < 0)
        return false;

    // Insert the segments where the clip sat, in order, then drop the clip's
    // own item. Deleting by pointer rather than index, since each insertion
    // shifts it along.
    int offset = 0;

    for (const auto& region : regions)
        playListContainer->createPlayListItemUI(region, insertAt + offset++);

    // The clip's region stays: it still describes the audio the segments came
    // from, and other items may reference it.
    return playListContainer->deletePlayListItem(original.get(), false);
}

bool AutoEdit::invokePython(const juce::File& audioFile,
                            AutoEditConfig &config,
                            std::function<void(std::string)> callback)
{
    const auto python = findPythonInterpreter();

    if (python.empty())
    {
        NullCheckedInvocation::invoke(callback,
                                      "No Python interpreter with gaborgandalf's dependencies was found. "
                                      "Install them into ~/.venvs/gaborgandalf, or point AUDIONAUT_PYTHON at "
                                      "an interpreter that has essentia, librosa and scipy < 1.15.");
        return false;
    }

    const auto audioFilePath = audioFile.getFullPathName().toStdString();

    // Build the command line string
    std::string commandString;
    commandString += "cd " + getTempDirectory().toStdString() + ";";

    // automain.py is run by path, which puts its own directory on sys.path but
    // not the parent - so `import gaborgandalf` needs the checkout added here.
    // Launched from Finder the app has no PYTHONPATH at all, so relying on the
    // shell's would work only when started from a terminal.
    commandString += "PYTHONPATH=\"$HOME/dev/gaborgandalf${PYTHONPATH:+:$PYTHONPATH}\" ";

    // --verbose
    commandString += "\"" + python + "\" $HOME/dev/gaborgandalf/gaborgandalf/automain.py autoedit";
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
