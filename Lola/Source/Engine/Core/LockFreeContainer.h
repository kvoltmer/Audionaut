//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <farbot/RealtimeTraits.hpp>
#include <farbot/fifo.hpp>

namespace audium
{


template <class _Tp>
class LockFreeContainer
{
private:
    
public:
    LockFreeContainer(int capacity) :
        fifo(capacity)
    {
    }
    
    ~LockFreeContainer() = default;

    static_assert (farbot::is_realtime_move_assignable<_Tp>::value);
    static_assert(! std::atomic<_Tp>::is_always_lock_free);
    
    std::vector<_Tp> &getProducerObjects ()
    {
        return producer_objects;
    }
    
    void clear ()
    {
        producer_objects.clear();
    }
    
    const std::vector<_Tp> &getConsumerObjects ()
    {
        return consumer_objects;
    }
    
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
    farbot::fifo<_Tp, farbot::fifo_options::concurrency::single, farbot::fifo_options::concurrency::single> fifo;
   
    std::vector<_Tp> producer_objects;
    std::vector<_Tp> consumer_objects;
        
    std::atomic<bool> objects_committed = false;
};

} // namespace audium
