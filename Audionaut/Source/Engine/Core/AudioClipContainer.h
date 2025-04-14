//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <array>
#include <concepts>

#include "Engine/Core/DspClipData.h"
#include "Engine/Core/LockFreeContainer.h"

namespace audium {

/**
 * @class AudioClipContainer
 * @brief A container for managing audio clip data using a lock-free mechanism.
 *
 * The `AudioClipContainer` class provides functionality to store, manage, and
 * process audio clip data (`DspClipData`) in a real-time safe manner. It uses
 * the `LockFreeContainer` class to ensure efficient and thread-safe operations.
 */
class AudioClipContainer {
    
public:
    /**
     * @brief Constructs an `AudioClipContainer` with a specified capacity.
     * @param capacity The maximum number of audio clips the container can hold.
     */
    AudioClipContainer(int capacity) :
        dspClips(capacity)
    {
    }
    
    /**
     * @brief Default destructor.
     */
    ~AudioClipContainer() = default;
    
    /**
     * @brief Clears all producer objects in the container.
     *
     * This method removes all audio clip data from the producer objects,
     * effectively resetting the container.
     */
    void clear()
    {
        dspClips.getProducerObjects().clear();
    }
    
    /**
     * @brief Adds a new audio clip to the producer objects.
     * @param clip The `DspClipData` object representing the audio clip to add.
     */
    void push_back(DspClipData clip)
    {
        dspClips.getProducerObjects().push_back(clip);
    }
    
    /**
     * @brief Commits the producer objects to the FIFO buffer.
     *
     * This method transfers all producer objects to the FIFO buffer, making
     * them available for consumption.
     */
    void commit()
    {
        dspClips.commit();
    }
    
    /**
     * @brief Pulls objects from the FIFO buffer to the consumer objects.
     * @return True if objects were successfully pulled, false otherwise.
     */
    bool pull()
    {
        return dspClips.pull();
    }
    
    /**
     * @brief Retrieves the consumer objects.
     * @return A const reference to the vector of consumer objects.
     */
    const std::vector<DspClipData> &getConsumerObjects()
    {
        return dspClips.getConsumerObjects();
    }
    
private:
    /**
     * @brief The lock-free container for managing `DspClipData` objects.
     */
    audium::LockFreeContainer<DspClipData> dspClips;
    
    /**
     * @brief JUCE macro to prevent copying and detect memory leaks.
     */
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioClipContainer)
    
};

} // namespace audium
