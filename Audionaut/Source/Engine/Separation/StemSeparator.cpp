//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Engine/Separation/StemSeparator.h"

#include "Engine/AudiumEngine.h"
#include "Engine/AudioSources/ClipFadeSpec.h"
#include "Engine/Export/AudioExporter.h"
#include "Engine/Export/ExportAudioConfig.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/Undo/UndoableEdit.h"

namespace audium {

namespace {

// How the progress range is shared between the steps of render().
constexpr double renderShare  = 0.05;
constexpr double separateShare = 0.90;

struct ResolvedClip
{
    std::shared_ptr<AudioTrack> track;
    std::shared_ptr<PlayListItem> item;
    std::shared_ptr<AudioRegion> region;
};

/// Looks the clip up by the ids in @p config, falling back to the track's
/// first clip when none is named - the convention AutoEdit uses.
ResolvedClip resolveClip (const AudioTrackContainer& container, const SeparationConfig& config)
{
    ResolvedClip resolved;
    resolved.track = container.getAudioTrack (config.trackId);

    if (resolved.track == nullptr)
        return resolved;

    auto playList = resolved.track->getPlayListContainer();

    if (playList == nullptr)
        return resolved;

    resolved.item = playList->getPlayListItem (config.playlistItemId);

    if (resolved.item == nullptr && config.playlistItemId < 0)
        resolved.item = playList->getPlayListItem (0);

    if (resolved.item != nullptr)
        resolved.region = resolved.item->getRegion();

    return resolved;
}

juce::File scratchRoot()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory)
               .getChildFile ("Audionaut")
               .getChildFile ("Separation");
}

bool readWholeFile (const juce::File& file, juce::AudioBuffer<float>& buffer, double& sampleRate, juce::String& error)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr)
    {
        error = "could not read the rendered clip";
        return false;
    }

    const auto numSamples = static_cast<int> (reader->lengthInSamples);

    if (numSamples <= 0)
    {
        error = "the rendered clip is empty";
        return false;
    }

    buffer.setSize (static_cast<int> (reader->numChannels), numSamples);
    reader->read (&buffer, 0, numSamples, 0, true, true);
    sampleRate = reader->sampleRate;
    return true;
}

bool writeStemFile (const juce::File& file, const juce::AudioBuffer<float>& stem, double sampleRate, juce::String& error)
{
    file.deleteFile();
    auto fileStream = std::make_unique<juce::FileOutputStream> (file);

    if (fileStream->failedToOpen())
    {
        error = "could not write " + file.getFullPathName();
        return false;
    }

    std::unique_ptr<juce::OutputStream> stream = std::move (fileStream);

    juce::WavAudioFormat wav;
    auto options = juce::AudioFormatWriter::Options{}
                       .withSampleRate (sampleRate)
                       .withNumChannels (stem.getNumChannels())
                       .withBitsPerSample (32)
                       .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);

    // Takes the stream over on success.
    auto writer = wav.createWriterFor (stream, options);

    if (writer == nullptr)
    {
        error = "could not create a WAV writer for " + file.getFullPathName();
        return false;
    }

    if (! writer->writeFromAudioSampleBuffer (stem, 0, stem.getNumSamples()))
    {
        error = "could not write " + file.getFullPathName();
        return false;
    }

    return true;
}

} // namespace

StemSeparator::StemSeparator (std::shared_ptr<AudiumEngine> engine,
                              std::shared_ptr<SeparationBackend> backend_) :
    audiumEngine (std::move (engine)),
    backend (std::move (backend_))
{
    jassert (audiumEngine != nullptr);
    jassert (backend != nullptr);
}

juce::String StemSeparator::stemTrackName (const juce::String& clipName, Stem stem)
{
    return clipName + " - " + stemDisplayName (stem);
}

bool StemSeparator::targetSelectedClip (SeparationConfig& config) const
{
    auto container = audiumEngine->getAudioTrackContainer();

    for (const auto& object : container->getSelectionManager()->getSelectedObjects())
        if (auto* item = dynamic_cast<PlayListItem*> (object.get()))
        {
            config.trackId = item->getRegion()->getAudioTrack()->getId();
            config.playlistItemId = item->getId();
            return true;
        }

    return false;
}

bool StemSeparator::canSeparate (const SeparationConfig& config, juce::String& reason) const
{
    if (! backend->isReady (reason))
        return false;

    if (audiumEngine->getPlayListScheduler()->isPlaying())
    {
        reason = "stop playback before separating stems";
        return false;
    }

    const auto resolved = resolveClip (*audiumEngine->getAudioTrackContainer(), config);

    if (resolved.track == nullptr)
    {
        reason = "no such track";
        return false;
    }

    if (resolved.item == nullptr || resolved.region == nullptr)
    {
        reason = "the track has no clip to separate";
        return false;
    }

    if (resolved.track->getNumAudioTrackChannels() > 2)
    {
        reason = "only clips on mono or stereo tracks can be separated";
        return false;
    }

    const auto spec = ClipFadeSpec::fromPlayListItem (*resolved.item);
    const auto length = spec.audibleLength();

    if (length <= 0.0)
    {
        reason = "the clip is empty";
        return false;
    }

    if (length > config.maxClipSeconds)
    {
        reason = "the clip is longer than the " + juce::String (juce::roundToInt (config.maxClipSeconds / 60.0))
                 + " minutes stem separation can handle";
        return false;
    }

    return true;
}

bool StemSeparator::prepare (const SeparationConfig& config, SeparationJob& job, juce::String& error) const
{
    if (! canSeparate (config, error))
        return false;

    const auto resolved = resolveClip (*audiumEngine->getAudioTrackContainer(), config);
    const auto spec = ClipFadeSpec::fromPlayListItem (*resolved.item);
    auto tempo = audiumEngine->getAudioTrackContainer()->getTempoProvider();

    job = {};
    job.clipName = resolved.region->getName();
    job.sourceTrackId = resolved.track->getId();
    job.sourceClipId = resolved.item->getId();
    job.numThreads = juce::jmax (1, config.numThreads);
    job.muteSourceTrack = config.muteSourceTrack;
    job.directory = scratchRoot().getChildFile (juce::Uuid().toString());

    // The render starts headExtension() ahead of the clip, so the stems have
    // to be placed that much earlier to line up.
    job.positionClocks = resolved.item->getAbsolutePosition (audium::clocks)
                         - tempo->secondsToClocks (spec.headExtension());

    // Mirror PlayListItemExport: a fresh item over the same region carrying
    // the clip's gain and fades, so the render sounds like the clip.
    auto exportConfig = std::make_shared<ExportAudioConfig>();
    exportConfig->playListItem = std::make_shared<PlayListItem> (resolved.item->getPlayListContainer(),
                                                                 resolved.region,
                                                                 resolved.track->getSelectionManager());
    exportConfig->playListItem->getDynamics().copyFrom (resolved.item->getDynamics());
    exportConfig->playListItem->setSpeedRatio (resolved.item->getSpeedRatio());
    exportConfig->numChannels = resolved.track->getNumAudioTrackChannels();
    exportConfig->sampleRate = backend->getRequiredSampleRate();
    exportConfig->bitDepth = 32;
    exportConfig->fileName = job.directory.getChildFile ("source.wav");

    job.exportConfig = exportConfig;
    return true;
}

bool StemSeparator::render (const SeparationJob& job,
                            const SeparationProgress& progress,
                            PendingStems& stems,
                            juce::String& error)
{
    stems = {};
    error.clear();

    if (job.exportConfig == nullptr)
    {
        error = "the separation was not prepared";
        return false;
    }

    if (! job.directory.createDirectory())
    {
        error = "could not create " + job.directory.getFullPathName();
        return false;
    }

    auto fail = [&job] (bool result)
    {
        job.directory.deleteRecursively();
        return result;
    };

    auto report = [&progress] (double fraction, const juce::String& message)
    {
        return progress == nullptr || progress (fraction, message);
    };

    // 1. Render the clip at the backend's rate.
    {
        AudioExporter exporter (*audiumEngine, job.exportConfig);
        exporter.bounce ([&report] (double fraction)
        {
            return report (renderShare * fraction, "Rendering clip");
        });

        if (job.exportConfig->userCanceled)
            return fail (false);
    }

    juce::AudioBuffer<float> rendered;
    double sampleRate = 0.0;

    if (! readWholeFile (job.exportConfig->fileName, rendered, sampleRate, error))
        return fail (false);

    // 2. The model wants stereo: duplicate a mono render.
    juce::AudioBuffer<float> stereo (2, rendered.getNumSamples());
    stereo.copyFrom (0, 0, rendered, 0, 0, rendered.getNumSamples());
    stereo.copyFrom (1, 0, rendered, rendered.getNumChannels() > 1 ? 1 : 0, 0, rendered.getNumSamples());

    std::vector<juce::AudioBuffer<float>> separated;

    const auto ok = backend->separate (stereo, separated,
                                       [&report] (double fraction, const juce::String& message)
                                       {
                                           return report (renderShare + separateShare * fraction, message);
                                       },
                                       error);

    if (! ok)
        return fail (false);

    if (static_cast<int> (separated.size()) != numStems)
    {
        error = "the separator returned " + juce::String (separated.size()) + " stems, expected " + juce::String (numStems);
        return fail (false);
    }

    // 3. One file per stem, named so the imported region and track carry the
    // clip's name. The backend always answers in stereo; a mono clip gets
    // mono stems back so the new tracks match the source - its two model
    // channels are near-identical (the input channels were), so averaging
    // them is transparent.
    const auto sourceIsMono = rendered.getNumChannels() == 1;

    for (auto index = 0; index < numStems; ++index)
    {
        const auto stem = stemFromIndex (index);
        const auto name = juce::File::createLegalFileName (stemTrackName (job.clipName, stem)) + ".wav";
        const auto file = job.directory.getChildFile (name);

        auto& separatedStem = separated[static_cast<size_t> (index)];
        juce::AudioBuffer<float> mono;

        if (sourceIsMono)
        {
            mono.setSize (1, separatedStem.getNumSamples());
            mono.copyFrom (0, 0, separatedStem, 0, 0, separatedStem.getNumSamples());
            mono.addFrom (0, 0, separatedStem, 1, 0, separatedStem.getNumSamples());
            mono.applyGain (0.5f);
        }

        if (! writeStemFile (file, sourceIsMono ? mono : separatedStem, sampleRate, error))
            return fail (false);

        stems.files[static_cast<size_t> (index)] = file;

        if (! report (renderShare + separateShare + (1.0 - renderShare - separateShare) * (index + 1) / numStems,
                      "Writing stems"))
            return fail (false);
    }

    job.exportConfig->fileName.deleteFile();

    stems.directory = job.directory;
    stems.positionClocks = job.positionClocks;
    stems.clipName = job.clipName;
    stems.sourceTrackId = job.sourceTrackId;
    stems.sourceClipId = job.sourceClipId;
    stems.muteSourceTrack = job.muteSourceTrack;
    return true;
}

bool StemSeparator::commit (const PendingStems& stems, std::vector<int>& newTrackIds, juce::String& error)
{
    newTrackIds.clear();

    for (const auto& file : stems.files)
        if (! file.existsAsFile())
        {
            error = "stem file missing: " + file.getFullPathName();
            return false;
        }

    auto container = audiumEngine->getAudioTrackContainer();

    const auto added = applyAsUndoableEdit (*container, [&]
    {
        std::vector<std::shared_ptr<AudioTrack>> created;

        for (auto index = 0; index < numStems; ++index)
        {
            const auto stem = stemFromIndex (index);
            std::string importError;

            const auto imported = container->addAudioFiles ({ stems.files[static_cast<size_t> (index)].getFullPathName() },
                                                            stems.positionClocks,
                                                            [&importError] (std::string message) { importError = message; },
                                                            false);

            if (! imported)
            {
                // Leave the project as it was: the transaction is dropped,
                // so the tracks added so far have to go by hand.
                for (const auto& track : created)
                    container->deleteAudioTrack (track);

                error = "could not import the " + stemDisplayName (stem) + " stem"
                        + (importError.empty() ? juce::String() : ": " + juce::String (importError));
                return false;
            }

            auto track = container->getAudioTracks().back();
            track->setAudioTrackName (stemTrackName (stems.clipName, stem));
            created.push_back (track);
            newTrackIds.push_back (track->getId());
        }

        // The stems replace the source in the mix: silence the source track
        // (inside this transaction, so undo unmutes it again).
        if (stems.muteSourceTrack)
            if (auto source = container->getAudioTrack (stems.sourceTrackId))
                for (auto channel = 0; channel < source->getNumAudioTrackChannels(); ++channel)
                    source->setMute (true, channel);

        return true;
    }, "Separate Stems");

    stems.directory.deleteRecursively();

    if (! added)
    {
        newTrackIds.clear();
        return false;
    }

    return true;
}

bool StemSeparator::separate (const SeparationConfig& config,
                              const SeparationProgress& progress,
                              std::vector<int>& newTrackIds,
                              juce::String& error)
{
    SeparationJob job;

    if (! prepare (config, job, error))
        return false;

    PendingStems stems;

    if (! render (job, progress, stems, error))
        return false;

    return commit (stems, newTrackIds, error);
}

void StemSeparator::discard (const PendingStems& stems)
{
    if (stems.directory != juce::File())
        stems.directory.deleteRecursively();
}

} // namespace audium
