
#pragma once

namespace audium
{

template <class _Tp, size_t _Size>
class LockFreeContainer
{
private:
    
public:
    LockFreeContainer() = default;
    ~LockFreeContainer() {
    }
    
    bool add (std::shared_ptr<_Tp> object)
    {
        if (objects.size() < _Size) {
            objects.push_back(object);
            commit();
            return true;
        }
        return false;
    }
    
    bool remove (std::shared_ptr<_Tp> object)
    {
        if (objects.size() > 0) {
            auto it = std::find_if(objects.begin(), objects.end(), [object](const auto& item) {
                return item == object;
            });
            
            if (it != objects.end()) {
                commit(object); // commit excluding the object to be removed.
                objects.erase(it);
                return true;
            }
        }
        
        return false;
    }
    
    const std::vector<std::shared_ptr<_Tp>> &getObjects () const
    {
        return objects;
    }
    
    void clear ()
    {
        objects.clear();
        commit();
    }
    
    const std::array<_Tp*, _Size> getObjectsLockFree () const
    {
        return lock_free_objects.load(std::memory_order_relaxed);
    }
    
private:
    
    void commit (std::shared_ptr<_Tp> excludeObject = nullptr)
    {
        std::array<_Tp*, _Size> values;
        
        for (std::size_t i = 0; i < _Size; i++) {
            if (i < objects.size() && objects[i] != excludeObject) {
                values[i] = objects[i].get();
            }
            else {
                values[i] = nullptr;
            }
        }
        
        lock_free_objects.store(values);
    }
    
    std::vector<std::shared_ptr<_Tp>> objects;
    
    std::atomic<std::array<_Tp*, _Size>> lock_free_objects;
    
};

} // namespace audium
