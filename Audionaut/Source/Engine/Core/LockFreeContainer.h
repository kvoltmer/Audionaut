//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <array>
#include <atomic>
#include <vector>

#include <farbot/RealtimeTraits.hpp>

namespace audium
{

/**
 * @class LockFreeContainer
 * @brief Hands whole snapshots of a vector from one producer thread to one
 *        consumer thread without locks or consumer-side allocation.
 *
 * The producer fills getProducerObjects() and commit()s; the consumer
 * pull()s and then reads getConsumerObjects() until its next pull(). A
 * commit replaces any snapshot the consumer has not pulled yet - the
 * consumer only ever sees the latest - and no snapshot is ever dropped
 * for size: the producer vectors grow on the producer thread as needed.
 *
 * Three slots rotate between the two threads: one the consumer reads, one
 * the producer fills, and one in the middle that carries the latest
 * snapshot across. Each side exchanges its own slot for the middle one on
 * a single atomic, so exactly one slot is ever in the middle and neither
 * side can take the other's - and the consumer never copies, moves or
 * allocates.
 *
 * @tparam _Tp The element type. Must be move-assignable in a real-time
 *             context.
 */
template <class _Tp>
class LockFreeContainer
{
public:
    /**
     * @brief Constructs a `LockFreeContainer`.
     * @param capacity The element count to reserve up front; the slots
     *        still grow (on the producer thread) beyond it when needed.
     */
    LockFreeContainer(int capacity)
    {
        for (auto& slot : slots)
            slot.reserve(static_cast<size_t>(capacity));

        producer_objects.reserve(static_cast<size_t>(capacity));
    }

    ~LockFreeContainer() = default;

    static_assert (farbot::is_realtime_move_assignable<_Tp>::value);

    /**
     * @brief Retrieves the producer objects (producer thread).
     * @return A reference to the vector of producer objects.
     */
    std::vector<_Tp> &getProducerObjects ()
    {
        return producer_objects;
    }

    /**
     * @brief Clears the producer objects (producer thread).
     */
    void clear ()
    {
        producer_objects.clear();
    }

    /**
     * @brief Retrieves the consumer objects (consumer thread): the snapshot
     *        the last pull() delivered, valid until the next pull().
     */
    const std::vector<_Tp> &getConsumerObjects ()
    {
        return slots[static_cast<size_t>(consumerSlot)];
    }

    /**
     * @brief Publishes a snapshot of the producer objects (producer thread).
     *
     * Copies the producer objects into the producer's slot and swaps it
     * into the middle, marked fresh; whatever was there (an unpulled
     * snapshot or a slot the consumer finished with) becomes the next
     * producer slot.
     */
    void commit ()
    {
        slots[static_cast<size_t>(producerSlot)] = producer_objects;

        const auto previous = middle.exchange(producerSlot | freshBit);
        producerSlot = previous & slotMask;
    }

    /**
     * @brief Takes the latest committed snapshot, if any (consumer thread).
     * @return True if a new snapshot was pulled, false if nothing was
     *         committed since the last pull.
     */
    /** Producer side: true once the consumer has taken over the last commit
        (so nothing it references only through an older snapshot is reachable). */
    bool isPulled () const noexcept
    {
        return (middle.load() & freshBit) == 0;
    }

    bool pull ()
    {
        // only the producer sets the fresh bit and only this clears it, so
        // once seen it stays set until the exchange below
        if ((middle.load() & freshBit) == 0)
            return false;

        const auto next = middle.exchange(consumerSlot);   // clean: no fresh bit
        consumerSlot = next & slotMask;
        return true;
    }

private:
    static constexpr int slotMask = 0x3;
    static constexpr int freshBit = 0x4;

    std::array<std::vector<_Tp>, 3> slots;

    std::vector<_Tp> producer_objects;

    int producerSlot = 0;               // producer thread only
    int consumerSlot = 1;               // consumer thread only
    std::atomic<int> middle { 2 };      // slot index, plus freshBit while unpulled
};

} // namespace audium
