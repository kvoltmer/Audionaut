//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <farbot/RealtimeTraits.hpp>
#include <farbot/fifo.hpp>

namespace audium
{

/**
 * @class LockFreeContainer
 * @brief A lock-free container for real-time data processing.
 *
 * The `LockFreeContainer` class provides a mechanism for managing data in a
 * lock-free manner, suitable for real-time audio applications. It uses a FIFO
 * buffer for efficient data transfer between producer and consumer objects.
 *
 * @tparam _Tp The type of objects stored in the container. Must be move-assignable
 *             in a real-time context.
 */
template <class _Tp>
class LockFreeContainer
{
public:
    /**
     * @brief Constructs a `LockFreeContainer` with a specified capacity.
     * @param capacity The maximum number of elements the container can hold.
     */
    LockFreeContainer(int capacity) :
        fifo(capacity)
    {
    }


    /**
     * @brief Default destructor.
     */
    ~LockFreeContainer() = default;

    
    static_assert (farbot::is_realtime_move_assignable<_Tp>::value);
    static_assert(! std::atomic<_Tp>::is_always_lock_free);
    
    /**
     * @brief Retrieves the producer objects.
     * @return A reference to the vector of producer objects.
     */
    std::vector<_Tp> &getProducerObjects ()
    {
        return producer_objects;
    }

    /**
     * @brief Clears the producer objects.
     */
    void clear ()
    {
        producer_objects.clear();
    }

    /**
     * @brief Retrieves the consumer objects.
     * @return A const reference to the vector of consumer objects.
     */
    const std::vector<_Tp> &getConsumerObjects ()
    {
        return consumer_objects;
    }

    /**
     * @brief Commits the producer objects to the FIFO buffer.
     *
     * This method transfers all producer objects to the FIFO buffer and marks
     * them as committed.
     */
    void commit ()
    {
        if (objects_committed.load()) {
            objects_committed.store(false);
            _Tp object;
            while (fifo.pop (object)) {
                ;
            }
        }
        
        for (_Tp object : producer_objects) {
            fifo.push(std::move(object));
        }
        
        objects_committed.store(true);
    }

    /**
     * @brief Pulls objects from the FIFO buffer to the consumer objects.
     * @return True if objects were successfully pulled, false otherwise.
     */
    bool pull ()
    {
        if (objects_committed.load()) {
            consumer_objects.clear();
            _Tp object;
            while (fifo.pop (object))
                consumer_objects.push_back(object);
            objects_committed.store(false);
            return true;
        }
        return false;
    }

private:
    /**
     * @brief The FIFO buffer for lock-free data transfer.
     */
    farbot::fifo<_Tp, farbot::fifo_options::concurrency::single, farbot::fifo_options::concurrency::single> fifo;

    /**
     * @brief The vector of producer objects.
     */
    std::vector<_Tp> producer_objects;

    /**
     * @brief The vector of consumer objects.
     */
    std::vector<_Tp> consumer_objects;

    /**
     * @brief Indicates whether objects have been committed to the FIFO buffer.
     */
    std::atomic<bool> objects_committed = false;
};

} // namespace audium
