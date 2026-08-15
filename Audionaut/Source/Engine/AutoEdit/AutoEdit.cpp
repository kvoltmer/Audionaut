//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <JuceHeader.h>

#include "AutoEdit.h"
#include "AutoEditParameter.h"
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
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Selection/SelectionManager.h"
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

    // Id of that clip (same numbering the dialog uses), or -1 when none.
    int playListItemId = -1;

    // The clip's name - its region's name, which is what the arrangement
    // shows on it. Empty when no clip resolved; the segments cut from the
    // clip are named after it.
    juce::String clipName;

    // The clip's absolute timeline position, in clocks. Only meaningful while
    // playListItemIndex >= 0 - without a clip the edit has no place on the
    // timeline.
    double clipStartClocks = 0.0;

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
                target.playListItemId = item->getId();
                target.clipStartClocks = item->getAbsolutePosition(audium::clocks);
                target.clipName = region->getName();
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

/**
 * The abstract measure parameter resolved against the analysed material: the
 * tempo comes from the file's cached beat analysis, so a measure spans what a
 * measure of that audio actually lasts. Falls back to the concrete numSegments
 * when the parameter is off or the file has no cached tempo (the merge's
 * analyses may predate BPM caching).
 */
int effectiveNumSegments(const AutoEditConfig& config,
                         const AnalysisProvider& analysisProvider,
                         const juce::File& audioFile,
                         double durationSeconds)
{
    const AutoEditParameter parameter(config.segmentMeasures);

    if (parameter.isActive())
    {
        const auto bpm = analysisProvider.getBpm(AnalysisType::BeatDegara, audioFile);
        const auto derived = parameter.numSegmentsFor(durationSeconds, bpm);

        if (derived > 0)
            return derived;
    }

    return config.numSegments;
}

/**
 * The merge parameters with the abstract measure parameter applied: the
 * segment count, and the minimum-length rule tightened (or relaxed) to the
 * derived lower bound so no segment falls below half the target length.
 * The upper bound has no merge-side rule to feed yet.
 */
EventMerger::Parameters effectiveMergeParameters(const AutoEditConfig& config,
                                                 const AnalysisProvider& analysisProvider,
                                                 const juce::File& audioFile,
                                                 double durationSeconds)
{
    EventMerger::Parameters params;
    params.numSegments = effectiveNumSegments(config, analysisProvider, audioFile, durationSeconds);

    const AutoEditParameter parameter(config.segmentMeasures);

    if (parameter.isActive())
    {
        const auto bpm = analysisProvider.getBpm(AnalysisType::BeatDegara, audioFile);
        const auto minSeconds = parameter.minSegmentSeconds(bpm);

        if (minSeconds > 0.0)
            params.minSegmentFrames = EventMerger::timeToFrame((float) minSeconds, params);
    }

    return params;
}

/**
 * The merged boundaries snapped onto the project's beat grid - but only when
 * the clip's beat analysis sits on that grid (the same
 * AnalysisProvider::matchesGrid() check that shows the dragger's check mark).
 * An off-grid clip keeps its analysed cut positions, losing only the ones
 * within a beat of the clip's edges - the sub-beat-sliver rule holds either
 * way. Left alone entirely when no clip anchors the edit to the timeline.
 */
std::vector<float> snapBoundariesForTarget(std::vector<float> boundaries,
                                           const EditTarget& target,
                                           const AnalysisProvider& analysisProvider,
                                           const AudioTrackContainer& audioTrackContainer,
                                           const juce::File& audioFile)
{
    if (target.playListItemIndex < 0)
        return boundaries;

    auto tempoProvider = audioTrackContainer.getTempoProvider();

    if (tempoProvider == nullptr)
        return boundaries;

    const auto projectTempo = tempoProvider->getTempo();
    const auto bpm = analysisProvider.getBpm(AnalysisType::BeatDegara, audioFile);
    const auto beats = analysisProvider.getSegments(AnalysisType::BeatDegara, audioFile);

    if (! AnalysisProvider::matchesGrid(projectTempo, bpm, beats,
                                        target.clipStartClocks, target.extent))
        return AutoEdit::skipEdgeBoundaries(boundaries, projectTempo, target.extent);

    return AutoEdit::snapBoundariesToGrid(boundaries, projectTempo,
                                          target.clipStartClocks,
                                          target.extent);
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

bool AutoEdit::targetSelectedClip(AutoEditConfig &config)
{
    auto audioTrackContainer = audiumEngine->getAudioTrackContainer();

    for (const auto& object : audioTrackContainer->getSelectionManager()->getSelectedObjects())
        if (auto* item = dynamic_cast<PlayListItem*>(object.get()))
        {
            config.trackId = item->getRegion()->getAudioTrack()->getId();
            config.playlistItemId = item->getId();
            return true;
        }

    return false;
}

void AutoEdit::targetSelection(AutoEditConfig &config)
{
    config.trackId = -1;
    config.playlistItemId = -1;

    if (targetSelectedClip(config))
        return;

    // No clip selected: the first clip of at least a second on the default
    // track, skipping slivers that are not worth editing.
    constexpr auto minLength = 1.0;

    if (auto track = audiumEngine->getAudioTrackContainer()->getDefaultGroup())
        for (const auto& item : track->getPlayListContainer()->getPlayListItems())
            if (item->getRegion()->getRegionData(audium::seconds).getLength() >= minLength)
            {
                config.trackId = track->getId();
                config.playlistItemId = item->getId();
                return;
            }
}

void AutoEdit::targetAssembleTrack(AssembleConfig &config)
{
    config.trackId = -1;

    auto audioTrackContainer = audiumEngine->getAudioTrackContainer();

    // The selected clip names the track being worked on, like the auto edit's
    // own targeting; without a selection the default track stands in.
    for (const auto& object : audioTrackContainer->getSelectionManager()->getSelectedObjects())
        if (auto* item = dynamic_cast<PlayListItem*>(object.get()))
        {
            config.trackId = item->getRegion()->getAudioTrack()->getId();
            return;
        }

    if (auto track = audioTrackContainer->getDefaultGroup())
        config.trackId = track->getId();
}

std::vector<int> AutoEdit::chooseRandomSequence(const std::vector<double>& lengthsSeconds,
                                                double targetSeconds,
                                                std::mt19937& rng)
{
    std::vector<int> chosen;

    if (lengthsSeconds.empty() || targetSeconds <= 0.0)
        return chosen;

    std::uniform_int_distribution<int> pick(0, (int) lengthsSeconds.size() - 1);

    // Every length is positive, so the accumulated total must reach the target
    // within this many picks - the cap only guards against a caller breaking
    // that precondition, where the loop would otherwise never end.
    const auto minLength = *std::min_element(lengthsSeconds.begin(), lengthsSeconds.end());
    const auto maxPicks = minLength > 0.0
                              ? (size_t) std::ceil(targetSeconds / minLength) + 1
                              : lengthsSeconds.size();

    double total = 0.0;

    while (total < targetSeconds && chosen.size() < maxPicks)
    {
        const auto index = pick(rng);
        chosen.push_back(index);
        total += lengthsSeconds[(size_t) index];
    }

    return chosen;
}

std::vector<int> AutoEdit::chooseSequentialSequence(const std::vector<double>& lengthsSeconds,
                                                    double targetSeconds,
                                                    std::mt19937& rng)
{
    std::vector<int> chosen;

    if (lengthsSeconds.empty() || targetSeconds <= 0.0)
        return chosen;

    const auto total = std::accumulate(lengthsSeconds.begin(), lengthsSeconds.end(), 0.0);

    if (total <= 0.0)
        return chosen;

    // gaborgandalf's thinning probability, which by itself only approximates
    // the target and can never exceed the material. Walking the segments in
    // order and wrapping around at the end until the target is reached keeps
    // the order and the thinning character, but actually delivers the
    // asked-for length - repeating the material when it is shorter than that.
    const auto keepProbability = std::min(1.0, targetSeconds / total);

    std::uniform_real_distribution<double> uniform01(0.0, 1.0);

    // Both caps only guard the pathological: every length is positive, so the
    // kept total must reach the target within maxKept picks, and a keep is
    // drawn about every 1 / keepProbability visits. An absurdly small target
    // can exhaust the visit cap and yield less than asked - or nothing.
    const auto minLength = *std::min_element(lengthsSeconds.begin(), lengthsSeconds.end());
    const auto maxKept = (size_t) std::ceil(targetSeconds / minLength) + 1;
    const auto maxVisits = std::min((size_t) 1000000,
                                    (size_t) std::ceil((double) maxKept / keepProbability) * 4 + 64);

    double accumulated = 0.0;

    for (size_t visit = 0; accumulated < targetSeconds && visit < maxVisits; ++visit)
    {
        // One draw per visited segment, kept or not - so a given seed always
        // produces the same keep-or-skip pattern.
        if (uniform01(rng) > keepProbability)
            continue;

        const auto index = (int) (visit % lengthsSeconds.size());
        chosen.push_back(index);
        accumulated += lengthsSeconds[(size_t) index];
    }

    return chosen;
}

bool AutoEdit::invokeAssemble(AssembleConfig &config,
                              std::function<void(std::string)> callback)
{
    auto audioTrackContainer = audiumEngine->getAudioTrackContainer();

    auto track = audioTrackContainer->getAudioTrack(config.trackId);

    if (track == nullptr)
    {
        NullCheckedInvocation::invoke(callback, "Please select a valid track to assemble.");
        return false;
    }

    if (config.duration <= 0.0)
    {
        NullCheckedInvocation::invoke(callback, "The song duration must be longer than zero seconds.");
        return false;
    }

    // The track's regions are the source material. A zero-length region would
    // make a clip that plays nothing (and could stall the random draw's
    // progress towards the target), so only regions with audio qualify.
    std::vector<std::shared_ptr<AudioRegion>> eligible;
    std::vector<double> lengths;

    for (const auto& region : track->getRegions())
    {
        const auto length = region->getRegionData(audium::seconds).getLength();

        if (length > 0.0)
        {
            eligible.push_back(region);
            lengths.push_back(length);
        }
    }

    if (eligible.empty())
    {
        NullCheckedInvocation::invoke(callback,
                                      "The track has no regions to assemble. Run Auto Edit first.");
        return false;
    }

    std::mt19937 rng(config.seed);

    const auto chosen = config.mode == AssembleConfig::Mode::Random
                            ? chooseRandomSequence(lengths, config.duration, rng)
                            : chooseSequentialSequence(lengths, config.duration, rng);

    // Only sequential mode can end up here: with a target well below the total
    // length, every draw may skip. Nothing has been touched yet, so the
    // arrangement is simply left as it was.
    if (chosen.empty())
    {
        NullCheckedInvocation::invoke(callback,
                                      "No regions were chosen - try again or use a longer duration.");
        return false;
    }

    return applyAsUndoableEdit([&track, &eligible, &chosen]
        {
            auto playListContainer = track->getPlayListContainer();

            if (playListContainer == nullptr)
                return false;

            // The old arrangement makes way entirely; the regions all stay -
            // they are the material being arranged. The item list is copied
            // first: it is a reference into the container being emptied.
            const auto items = playListContainer->getPlayListItems();

            for (const auto& item : items)
                if (! playListContainer->deletePlayListItem(item.get(), false))
                    return false;

            // The chosen regions become the new arrangement, butt-joined from
            // the start of the timeline in the order the mode chose. Random
            // mode repeats regions, so several items may share one region.
            auto positionClocks = 0.0;

            for (auto index : chosen)
                if (auto item = playListContainer->createPlayListItemAtPositionUI(eligible[(size_t) index],
                                                                                  positionClocks,
                                                                                  audium::clocks))
                    positionClocks += item->getRegionData(audium::clocks).getLength();

            playListContainer->sortByPosition();

            return true;
        },
        "Assemble");
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

    const auto params = effectiveMergeParameters(config, *analysisProvider, audioFile, duration);

    auto result = analysisProvider->mergeCachedAnalyses(audioFile, (float) duration, params);

    // One boundary bounds no region - two are needed before anything can be cut.
    if (result.boundaries.size() < 2)
    {
        NullCheckedInvocation::invoke(callback, "No segment boundaries were found in this audio.");
        return false;
    }

    const auto boundaries = snapBoundariesForTarget(std::move(result.boundaries),
                                                    target,
                                                    *analysisProvider,
                                                    *audioTrackContainer,
                                                    audioFile);

    // The analyses describe the whole file; the edit applies only to what the
    // chosen clip covers. Without a clip to go on, that is the whole file.
    const auto extent = target.extent.isEmpty() ? juce::Range<double>(0.0, duration)
                                                : target.extent;

    const auto replaceClip = config.replacePlayListItem;
    const auto clipIndex = target.playListItemIndex;

    // Segments are named after the clip they are cut from; without one, the
    // track name stands in.
    const auto baseName = target.clipName.isNotEmpty() ? target.clipName
                                                       : track->getAudioTrackName();

    if (! applyAsUndoableEdit([this, &boundaries, extent, track, resourceGroup,
                               replaceClip, clipIndex, baseName]
        {
            auto created = createRegionsFromBoundaries(boundaries, extent, track,
                                                       resourceGroup, baseName);

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

bool AutoEdit::previewAutoEdit(AutoEditConfig &config)
{
    auto audioTrackContainer = audiumEngine->getAudioTrackContainer();

    auto analysisProvider = audioTrackContainer->getAnalysisProvider();

    if (analysisProvider == nullptr)
        return false;

    auto track = audioTrackContainer->getAudioTrack(config.trackId);
    const auto target = resolveEditTarget(track, config.playlistItemId);

    if (! target.isValid() || config.source != AutoEditConfig::Source::Native)
    {
        analysisProvider->clearMergePreview();
        return false;
    }

    const auto audioFile = juce::File(target.resource->getFullPathName());
    const auto duration = target.resource->getFileLength(audium::seconds);

    if (! audioFile.existsAsFile()
        || duration <= 0.0
        || ! analysisProvider->findMissingMergeAnalyses(audioFile).empty())
    {
        analysisProvider->clearMergePreview();
        return false;
    }

    const auto params = effectiveMergeParameters(config, *analysisProvider, audioFile, duration);

    auto result = analysisProvider->mergeCachedAnalyses(audioFile, (float) duration, params);

    // Snapped the same way invokeAutoEdit() snaps, so the preview shows the
    // cuts the edit would actually make.
    auto boundaries = snapBoundariesForTarget(std::move(result.boundaries),
                                              target,
                                              *analysisProvider,
                                              *audioTrackContainer,
                                              audioFile);

    // Only boundaries that become cuts are published: the merge always emits
    // the file's own edges, which createRegionsFromBoundaries drops - shown in
    // the preview they would read as an edit right at the clip's start or end
    // that invoking the edit never makes.
    const auto extent = target.extent.isEmpty() ? juce::Range<double>(0.0, duration)
                                                : target.extent;

    std::erase_if(boundaries, [&extent] (float boundary)
    {
        const auto point = (double) boundary;
        return point <= extent.getStart() || point >= extent.getEnd();
    });

    analysisProvider->setMergePreview(audioFile.getFullPathName().toStdString(),
                                      track->getId(),
                                      target.playListItemId,
                                      std::move(boundaries),
                                      config.segmentMeasures);
    return true;
}

int AutoEdit::resolveNumSegments(AutoEditConfig &config)
{
    auto audioTrackContainer = audiumEngine->getAudioTrackContainer();
    auto analysisProvider = audioTrackContainer->getAnalysisProvider();

    const auto target = resolveEditTarget(audioTrackContainer->getAudioTrack(config.trackId),
                                          config.playlistItemId);

    if (analysisProvider == nullptr || ! target.isValid())
        return config.numSegments;

    const auto audioFile = juce::File(target.resource->getFullPathName());
    const auto duration = target.resource->getFileLength(audium::seconds);

    const auto params = effectiveMergeParameters(config, *analysisProvider, audioFile, duration);

    if (duration <= 0.0 || ! analysisProvider->findMissingMergeAnalyses(audioFile).empty())
        return params.numSegments;

    // The merge parameter describes the whole file, but only what falls inside
    // the clip is cut. Counting the merged boundaries the same way
    // createRegionsFromBoundaries filters them - after the same grid snapping
    // invokeAutoEdit() applies - gives the count the edit would actually
    // produce.
    auto result = analysisProvider->mergeCachedAnalyses(audioFile, (float) duration, params);

    const auto boundaries = snapBoundariesForTarget(std::move(result.boundaries),
                                                    target,
                                                    *analysisProvider,
                                                    *audioTrackContainer,
                                                    audioFile);

    const auto extent = target.extent.isEmpty() ? juce::Range<double>(0.0, duration)
                                                : target.extent;

    int inside = 0;

    for (auto boundary : boundaries)
    {
        const auto point = (double) boundary;

        if (point > extent.getStart() && point < extent.getEnd())
            ++inside;
    }

    // No boundary inside the clip means nothing gets cut, not one big segment.
    return inside > 0 ? inside + 1 : 0;
}

juce::Range<double> AutoEdit::resolveSegmentLengthBounds(AutoEditConfig &config)
{
    const AutoEditParameter parameter(config.segmentMeasures);

    if (! parameter.isActive())
        return {};

    auto audioTrackContainer = audiumEngine->getAudioTrackContainer();
    auto analysisProvider = audioTrackContainer->getAnalysisProvider();

    const auto target = resolveEditTarget(audioTrackContainer->getAudioTrack(config.trackId),
                                          config.playlistItemId);

    if (analysisProvider == nullptr || ! target.isValid())
        return {};

    const auto audioFile = juce::File(target.resource->getFullPathName());
    const auto bpm = analysisProvider->getBpm(AnalysisType::BeatDegara, audioFile);

    // Both bounds are zero when the tempo is unknown, which is the empty range
    // the caller checks for.
    return { parameter.minSegmentSeconds(bpm), parameter.maxSegmentSeconds(bpm) };
}

bool AutoEdit::applyAsUndoableEdit(std::function<bool()> mutate,
                                   const juce::String& transactionName)
{
    auto audioTrackContainer = audiumEngine->getAudioTrackContainer();
    auto action = std::make_unique<audium::UndoableContainerAction>(*audioTrackContainer.get());

    if (! mutate())
        return false;

    // Undo: store new state, so a single Undo takes back everything this edit
    // changed.
    action->storeNewState();
    audioTrackContainer->getUndoManager()->perform(action.release(), transactionName);
    audioTrackContainer->getUndoManager()->beginNewTransaction();

    return true;
}

std::vector<float> AutoEdit::snapBoundariesToGrid(const std::vector<float>& boundarySeconds,
                                                  double projectTempoBpm,
                                                  double clipStartClocks,
                                                  juce::Range<double> playedRegionSeconds)
{
    const auto regionStart = playedRegionSeconds.getStart();

    const auto toTimelineBeats = [&] (double fileSeconds)
    {
        return TempoProvider::clocksToBeats(clipStartClocks
            + TempoProvider::secondsToClocks(projectTempoBpm, fileSeconds - regionStart));
    };

    // The grid beats a snapped cut may land on: at least one beat away from
    // either edge of the clip, so no cut can bound a segment shorter than a
    // beat against the clip's start or end.
    const auto firstAllowedBeat = std::ceil(toTimelineBeats(regionStart) + 1.0);
    const auto lastAllowedBeat  = std::floor(toTimelineBeats(playedRegionSeconds.getEnd()) - 1.0);

    std::vector<float> snapped;
    snapped.reserve(boundarySeconds.size());

    for (auto boundary : boundarySeconds)
    {
        const auto fileSeconds = (double) boundary;

        // Edge and outside boundaries make no cut either way - passing them
        // through unchanged keeps a cut from being invented where the
        // analysis put none.
        if (fileSeconds <= regionStart || fileSeconds >= playedRegionSeconds.getEnd())
        {
            snapped.push_back(boundary);
            continue;
        }

        const auto beat = std::round(toTimelineBeats(fileSeconds));

        // A cut whose nearest beat sits within a beat of the clip's start or
        // end is skipped: it would leave a sub-beat sliver of a segment.
        if (beat < firstAllowedBeat || beat > lastAllowedBeat)
            continue;

        snapped.push_back((float) (regionStart
            + TempoProvider::clocksToSeconds(projectTempoBpm,
                                             TempoProvider::beatsToClocks(beat) - clipStartClocks)));
    }

    // Two boundaries can snap onto the same beat; the duplicate would only
    // bound a zero-length segment.
    std::sort(snapped.begin(), snapped.end());
    snapped.erase(std::unique(snapped.begin(), snapped.end()), snapped.end());

    return snapped;
}

std::vector<float> AutoEdit::skipEdgeBoundaries(const std::vector<float>& boundarySeconds,
                                                double projectTempoBpm,
                                                juce::Range<double> playedRegionSeconds)
{
    std::vector<float> kept;
    kept.reserve(boundarySeconds.size());

    for (auto boundary : boundarySeconds)
    {
        const auto fileSeconds = (double) boundary;

        // On or outside the edges: no cut either way, filtered later.
        if (fileSeconds <= playedRegionSeconds.getStart()
            || fileSeconds >= playedRegionSeconds.getEnd())
        {
            kept.push_back(boundary);
            continue;
        }

        const auto startDistanceBeats = TempoProvider::secondsToBeats(
            projectTempoBpm, fileSeconds - playedRegionSeconds.getStart());
        const auto endDistanceBeats = TempoProvider::secondsToBeats(
            projectTempoBpm, playedRegionSeconds.getEnd() - fileSeconds);

        if (startDistanceBeats < 1.0 || endDistanceBeats < 1.0)
            continue;

        kept.push_back(boundary);
    }

    return kept;
}

std::vector<std::shared_ptr<AudioRegion>>
    AutoEdit::createRegionsFromBoundaries(const std::vector<float>& boundarySeconds,
                                          juce::Range<double> extent,
                                          std::shared_ptr<AudioTrack> track,
                                          std::shared_ptr<ResourceGroup> resourceGroup,
                                          const juce::String& baseName)
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

    for (size_t i = 1; i < points.size(); ++i)
    {
        juce::Range<double> position;
        position.setStart(points[i - 1]);
        position.setEnd(points[i]);

        // Named <clip>-seg-<number>, numbered by the container's own
        // unique-name mechanism - each created region advances the number for
        // the next.
        const auto regionName = regionContainer->getUniqueName(baseName + "-seg");

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

    const auto originalStartClocks = original->getAbsolutePosition(audium::clocks);

    // The clip's item goes first: inserting the segments while it still sat
    // there would make way for each of them, pushing the clip - and every
    // neighbouring item behind it - down the timeline, and only the segments
    // would be moved back. The clip's region stays: it still describes the
    // audio the segments came from, and other items may reference it.
    if (! playListContainer->deletePlayListItem(original.get(), false))
        return false;

    // The segments take the clip's place on the timeline: the first sits
    // exactly where the clip sat and the rest follow seamlessly. Placing them
    // by position rather than by index leaves the neighbouring items where
    // they are - the segments only ever cover the stretch the clip covered.
    auto positionClocks = originalStartClocks;

    for (const auto& region : regions)
        if (auto item = playListContainer->createPlayListItemAtPositionUI(region,
                                                                          positionClocks,
                                                                          audium::clocks))
            positionClocks += item->getRegionData(audium::clocks).getLength();

    playListContainer->sortByPosition();

    return true;
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

    // Segments are named after the clip they are cut from; without one, the
    // track name stands in.
    const auto regionBaseName = target.clipName.isNotEmpty()
                                    ? target.clipName
                                    : target.track->getAudioTrackName();

    return applyAsUndoableEdit([this, segFileName, sampleRate, target, regionBaseName]
    {
        return createRegionsFromSegFile(segFileName, sampleRate, target.track,
                                        target.resourceGroup, regionBaseName);
    });
}

const std::string AutoEdit::getBaseName() const
{
    return juce::File(audioResourceFilePath).getFileNameWithoutExtension().toStdString();
}

bool AutoEdit::createRegionsFromSegFile(std::string segFileName,
                                       double sampleRate,
                                       std::shared_ptr<AudioTrack> track,
                                       std::shared_ptr<ResourceGroup> resourceGroup,
                                       const juce::String& baseName)
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

    auto created = 0;
    auto segdata = nlohmann::json::parse(segFile);
    auto regionContainer = resourceGroup->getAudioRegionContainer();

    // create regions from parsed result
    for (auto& elem : segdata)
    {
        juce::Range<double> position;
        position.setStart(static_cast<double>(elem["start"]) / sampleRate);
        position.setEnd(static_cast<double>(elem["end"]) / sampleRate);

        // Named <clip>-seg-<number>, numbered by the container's own
        // unique-name mechanism - each created region advances the number for
        // the next.
        const auto regionName = regionContainer->getUniqueName(baseName + "-seg");

        if (regionContainer->createRegion(regionName,
                                          position,
                                          track,
                                          resourceGroup,
                                          nullptr,
                                          audium::seconds))
            ++created;
    }

    segFile.close();
    return created > 0;
}

} // namespace audium
