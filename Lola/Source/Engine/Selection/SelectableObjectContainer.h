/*
  ==============================================================================

    SelectableObjectContainer.h
    Created: 6 Oct 2024 10:54:16am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

namespace audium
{

template <typename ElementType>
class SelectableObjectContainer
{
private:
    
public:
    SelectableObjectContainer() = default;
    
    std::vector<std::shared_ptr<ElementType>> getObjects() const { return objects; }
    
    bool deleteObject(ElementType* object) {
        auto it = std::find_if(objects.begin(), objects.end(), [object](const auto& item) {
            return item.get() == object;
        });
        
        if (it != objects.end()) {
            object->cleanup();
            objects.erase(it);
            return true;
        }
        
        return false;
    }
    
    void selectAllObjects(bool bSelected) {
        for (auto object : objects)
            object->setSelected(bSelected, false);
    }
    
    void cleanup() {
        for (auto object : objects)
            object->cleanup();
    }
    
    void push_back(std::shared_ptr<ElementType> object) {
        objects.push_back(object);
    }
    
private:
    std::vector<std::shared_ptr<ElementType>> objects;
};

} // namespace audium
