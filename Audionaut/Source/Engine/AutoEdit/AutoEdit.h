//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <JuceHeader.h>

namespace audium {


class AudiumEngine;
class AudioTrack;
class PlayListContainer;
class AudioTrackContainer;
class PlayListItem;
class ResourceGroup;

struct AutoEditConfig {
    /**
     * @brief Where the segment boundaries come from.
     */
    enum class Source {
        /// AnalysisProvider + EventMerger, merged from the cached analyses.
        Native,
        /// The gaborgandalf Python subprocess. Retained so its boundaries can
        /// be compared against the native ones on the same material; unlike the
        /// native path it needs a working gaborgandalf environment.
        Python
    };

    Source source = Source::Native;
    std::string mode = "random";
    double duration = 120.0;
    int numSegments = 20;
    double minSegLength = 2.0;
    double maxSegLength = 60.0;
    int trackId = -1;
    int playlistItemId = -1;
};

/**
 * @class AutoEdit
 * @brief Cuts a track's audio into regions at automatically chosen boundaries.
 *
 * The native path merges the analyses already held in the `AnalysisCache` -
 * BIC segmentation, Degara beat tracking and onsets - through `EventMerger`,
 * which weights rare structural boundaries above frequent beats so the cuts
 * land on beat-aligned segment boundaries.
 *
 * Nothing is rendered and no temporary files are written: the analyses describe
 * the track's source audio file, which is also what the regions are created
 * against, so boundaries and regions share one timeline.
 */
class AutoEdit {

public:
    AutoEdit(std::shared_ptr<AudiumEngine> audiumEngine_) :
        audiumEngine(audiumEngine_)
    {}

    /**
     * @brief Runs an auto edit over the track named by @p config.
     *
     * @param config   What to edit, and which source to take boundaries from.
     * @param callback Invoked with a human-readable reason when the edit cannot
     *                 run - e.g. the track's analyses are not cached yet.
     * @return True when regions were created.
     */
    bool invokeAutoEdit(AutoEditConfig &config,
                        std::function<void(std::string)> callback);

    /**
     * @brief Runs the gaborgandalf subprocess over @p audioFile and turns the
     *        segment file it writes into regions.
     */
    bool invokePython(const juce::File& audioFile,
                      AutoEditConfig &config,
                      std::function<void(std::string)> callback);

    /**
     * @brief Creates one region per consecutive pair of boundaries.
     *
     * @param boundarySeconds Ascending boundary times, in seconds.
     * @param track           The track to create the regions on.
     * @param resourceGroup   The resource group the regions belong to.
     * @return True when at least one region was created.
     */
    bool createRegionsFromBoundaries(const std::vector<float>& boundarySeconds,
                                     std::shared_ptr<AudioTrack> track,
                                     std::shared_ptr<ResourceGroup> resourceGroup);

    /** @brief Reads the Python path's segment file and creates regions from it. */
    bool createRegionsFromSegFile(std::string segFileName, double sampleRate);

    static const juce::String getTempDirectory();

private:
    std::shared_ptr<AudiumEngine> audiumEngine;

    std::string audioResourceFilePath;

    const std::string getBaseName() const;

    // Wraps region creation in the undo transaction the UI expects, so one
    // Undo removes every region an edit produced.
    bool applyAsUndoableEdit(std::function<bool()> createRegions);
};

} // namespace audium

