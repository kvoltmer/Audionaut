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
    ~SelectableObjectContainer() {
        jassert(objects.size() == 0);
    }
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
    
    bool objectExists(ElementType* object) const {
        auto it = std::find_if(objects.begin(), objects.end(), [object](const auto& item) {
            return item.get() == object;
        });
        return it != objects.end();
    }
    
    void selectAllObjects(bool bSelected) {
        for (auto object : objects)
            object->setSelected(bSelected, false);
    }
    
    void cleanup() {
        for (auto object : objects)
            object->cleanup();
        objects.clear();
    }
    
    void push_back(std::shared_ptr<ElementType> object) {
        objects.push_back(object);
    }
    
    size_t size() const {
        return objects.size();
    }
    
    juce::SparseSet<int> getSelectedRows() const {
        juce::SparseSet<int> result;
        for (auto i = 0; i < size(); i++) {
            if (objects[i] != nullptr &&
                objects[i]->isSelected()) {
                result.addRange ({i, i + 1});
            }
        }
        return result;
    }
    
    void setSelectedRows(juce::SparseSet<int>& selectedRows) {
        selectAllObjects(false);
        for (auto i = 0; i < selectedRows.size(); i++)
        {
            if (auto object = objects[selectedRows[i]])
            {
                object->setSelected(true, false);
            }
        }
    }
    
    std::vector<std::shared_ptr<ElementType>> objects;
};

} // namespace audium
