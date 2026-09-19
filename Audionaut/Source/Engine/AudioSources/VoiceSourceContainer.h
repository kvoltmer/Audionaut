//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <JuceHeader.h>

#include "Engine/Core/LockFreeContainer.h"

namespace audium {

class VoiceSource;
class AudioResource;
class Playback;

/**
 * @class VoiceSourceContainer
 * @brief Owns a track's voice sources and publishes them to the audio thread.
 *
 * Ownership lives on the message thread. The audio thread never touches the
 * owning vector: it reads a triple-buffered snapshot of raw pointers that is
 * published by commit() (PlayListScheduler::commitPlayListData) and taken
 * over by pull() at the start of each block - the same generation scheme as
 * AudioClipContainer, and committed right before it so the clips' indices
 * always refer to the voices of the same generation.
 *
 * Indices are stable: a removed voice leaves a null slot instead of
 * shifting its neighbours, so a clip snapshot one generation behind can at
 * worst hit a null and skip. Removed voice sources are retired, not
 * destroyed: their memory is released on the message thread only once the
 * audio thread has pulled a snapshot without them and no Voice renders them.
 */
class VoiceSourceContainer
{
public:
    explicit VoiceSourceContainer(std::shared_ptr<Playback> playback_) :
        playback(std::move(playback_))
    {}

    ~VoiceSourceContainer() = default;

    // ---- message thread -------------------------------------------------

    void prepareToPlay (int samplesPerBlockExpected,
                        double sampleRate);

    /** Stops and retires every voice source. */
    void cleanup();

    std::shared_ptr<VoiceSource> createAndAddVoiceSource(AudioResource& audioResource,
                                                         std::shared_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource);

    /** Stops the voice, frees its index and retires it. Returns false if unknown. */
    bool removeVoiceSource(std::shared_ptr<VoiceSource> voiceSource);

    std::vector<std::shared_ptr<VoiceSource>> getVoiceSourcesForResource(const AudioResource &resource) const;

    /** Stable index of a live voice source, -1 if unknown or removed. */
    int getVoiceSourceIndex(std::shared_ptr<VoiceSource> searchVoiceSource) const;

    void applyChannelMapping();

    /** Publishes the current set to the audio thread and releases retired
        sources the audio thread can no longer reach. */
    void commit();

    /** Number of retired sources still waiting for the audio thread to let go (tests). */
    size_t getNumRetired() const noexcept { return retiredPending.size() + retiredCommitted.size(); }

    // ---- audio thread ---------------------------------------------------

    /** Takes over the latest committed snapshot. Returns true if it changed. */
    bool pull();

    /** Voice source at a stable index in the pulled snapshot, nullptr if none. */
    VoiceSource* getVoiceSourceAtIndex(int index) const noexcept;

private:
    void releaseRetired();

    std::shared_ptr<Playback> playback; ///< Shared pointer to the Playback instance.

    std::vector<std::shared_ptr<VoiceSource>> voiceSources; ///< Owning set; null = freed slot, never erased.
    std::vector<std::shared_ptr<VoiceSource>> retiredPending;   ///< removed since the last commit
    std::vector<std::shared_ptr<VoiceSource>> retiredCommitted; ///< excluded from the last committed snapshot

    static constexpr int initialCapacity = 1024;
    mutable LockFreeContainer<VoiceSource*> snapshot { initialCapacity };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceSourceContainer)
};

} // namespace audium
