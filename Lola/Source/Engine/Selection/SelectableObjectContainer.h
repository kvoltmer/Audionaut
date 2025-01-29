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
    
    const std::vector<std::shared_ptr<ElementType>> &getObjects() const { return objects; }
    
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
    
    bool objectExistsAtIndex(int index) const {
        return (index >= 0 && index < objects.size());
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
    
    int getIndex(std::shared_ptr<const ElementType> searchObject) const
    {
        auto it = std::find(objects.begin(), objects.end(), searchObject);
        if (it != objects.end())
            return static_cast<int>(std::distance(objects.begin(), it));

        return -1;
    }
    
    std::vector<std::shared_ptr<ElementType>> objects;
};



template<typename C>
void MoveItemBefore(C& container, size_t currentIndex, size_t indexOfItemToPlaceBefore)
{
    if( currentIndex == indexOfItemToPlaceBefore ) return;
    
    jassert( juce::isPositiveAndBelow((int)currentIndex, (int)container.size() ));
    jassert( juce::isPositiveAndBelow((int)indexOfItemToPlaceBefore, (int)container.size() + 1 ));
    
    if (currentIndex < indexOfItemToPlaceBefore)
    {
        std::rotate(container.begin() + currentIndex,
                    container.begin() + currentIndex + 1,
                    container.begin() + indexOfItemToPlaceBefore);
    }
    else
    {
        std::rotate(container.begin() + indexOfItemToPlaceBefore,
                    container.begin() + currentIndex,
                    container.begin() + currentIndex + 1);
    }
}

} // namespace audium
