//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <JuceHeader.h>

namespace audium {

/// Where a model comes from and how to recognise a good copy of it.
struct DemucsModelInfo
{
    juce::String fileName;
    juce::URL url;
    juce::String sha256Hex;
    juce::int64 expectedBytes = 0;
};

/**
 * Keeps the Demucs weights file on disk: knows where it lives, fetches it,
 * verifies it and removes it again.
 *
 * The weights are not part of the application - they are downloaded once,
 * on first use, into the user's application-data directory. Every install
 * path verifies the SHA-256 before the file is put in place, so a partial or
 * tampered download never becomes "the model".
 */
class DemucsModelStore
{
public:
    /// @param directory  Where the model file is kept. Tests pass a temp dir.
    explicit DemucsModelStore (juce::File directory);

    /// The htdemucs 4-source weights this build expects.
    static const DemucsModelInfo& defaultModel();

    /// Where the application keeps its models: a "Models" folder in the
    /// per-user application-data directory.
    static juce::File defaultDirectory();

    /// A store with the default model in the default directory.
    static DemucsModelStore createDefault();

    /// Use a different model description (tests substitute a small file).
    void setModel (DemucsModelInfo info);

    const DemucsModelInfo& getModel() const { return model; }

    juce::File getDirectory() const { return directory; }

    /// The model's path, whether or not it exists yet.
    juce::File getModelFile() const;

    /// True when a file of the expected size is in place. The hash is checked
    /// on install, not here: hashing 84 MB on every menu update is too slow.
    bool isAvailable() const;

    /// Checks @p file against the model's SHA-256.
    bool verify (const juce::File& file, juce::String& error) const;

    /**
     * Progress of a download.
     *
     * @param bytesDone  Bytes received so far.
     * @param bytesTotal Size announced by the server, or -1 if unknown.
     * @return           false to cancel.
     */
    using DownloadProgress = std::function<bool (juce::int64 bytesDone, juce::int64 bytesTotal)>;

    /**
     * Downloads the model into the store. Blocks; call from a worker thread.
     *
     * Streams into a ".part" file next to the target, verifies it, then
     * renames it into place. On failure or cancellation nothing is left
     * behind.
     *
     * @return true when the model is in place afterwards.
     */
    bool download (const DownloadProgress& progress, juce::String& error);

    /// Installs a copy of @p source after verifying it (for offline use and
    /// tests).
    bool installFromFile (const juce::File& source, juce::String& error);

    /// Deletes the installed model, if any.
    bool remove();

private:
    bool moveIntoPlace (const juce::File& candidate, juce::String& error);

    juce::File directory;
    DemucsModelInfo model;
};

} // namespace audium
