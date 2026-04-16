//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

namespace audium
{

/**
 * @class SelectableObjectContainer
 * @brief A container class for managing a collection of selectable objects.
 *
 * This template class provides functionality to manage a collection of objects
 * that can be selected, deleted, and manipulated. It ensures proper cleanup
 * and integrates with selection logic.
 *
 * @tparam ElementType The type of objects stored in the container.
 */
template <typename ElementType>
class SelectableObjectContainer
{
public:
    std::vector<std::shared_ptr<ElementType>> objects; ///< The collection of objects.

public:
    /**
     * @brief Default constructor.
     */
    SelectableObjectContainer() = default;

    /**
     * @brief Destructor. Ensures the container is empty upon destruction.
     */
    ~SelectableObjectContainer() {
        jassert(objects.size() == 0);
    }

    /**
     * @brief Gets the collection of objects.
     * @return A const reference to the vector of objects.
     */
    const std::vector<std::shared_ptr<ElementType>>& getObjects() const { return objects; }

    /**
     * @brief Deletes an object from the container.
     * @param object A pointer to the object to delete.
     * @return True if the object was found and deleted, false otherwise.
     */
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

    /**
     * @brief Checks if an object exists in the container.
     * @param object A pointer to the object to check.
     * @return True if the object exists, false otherwise.
     */
    bool objectExists(ElementType* object) const {
        auto it = std::find_if(objects.begin(), objects.end(), [object](const auto& item) {
            return item.get() == object;
        });
        return it != objects.end();
    }
    
    /**
     * @brief get an object by index
     * @param
     * @return maybe nullptr
     */
    std::shared_ptr<ElementType> getObject(size_t index) const {
        if (index >= 0 &&
            index < size()) {
            return objects[index];
        }
        return nullptr;
    }

    /**
     * @brief Checks if an object exists at a specific index.
     * @param index The index to check.
     * @return True if an object exists at the index, false otherwise.
     */
    bool objectExistsAtIndex(int index) const {
        return (index >= 0 && index < objects.size());
    }

    /**
     * @brief Selects or deselects all objects in the container.
     * @param bSelected True to select all objects, false to deselect them.
     */
    void selectAllObjects(bool bSelected) {
        for (auto object : objects)
            object->setSelected(bSelected, false);
    }

    /**
     * @brief Cleans up all objects in the container and clears the collection.
     */
    void cleanup() {
        for (auto object : objects) {
            if (object != nullptr)
                object->cleanup();
        }
        objects.clear();
    }

    /**
     * @brief Adds an object to the container.
     * @param object A shared pointer to the object to add.
     */
    void push_back(std::shared_ptr<ElementType> object) {
        objects.push_back(object);
    }

    /**
     * @brief Gets the number of objects in the container.
     * @return The size of the container.
     */
    size_t size() const {
        return objects.size();
    }

    /**
     * @brief Gets the indices of selected objects as a SparseSet.
     * @return A SparseSet containing the indices of selected objects.
     */
    juce::SparseSet<int> getSelectedRows() const {
        juce::SparseSet<int> result;
        for (auto i = 0; i < size(); i++) {
            if (objects[i] != nullptr &&
                objects[i]->isSelected()) {
                result.addRange({i, i + 1});
            }
        }
        return result;
    }

    /**
     * @brief Sets the selection state of objects based on a SparseSet of indices.
     * @param selectedRows A SparseSet containing the indices to select.
     */
    void setSelectedRows(juce::SparseSet<int>& selectedRows) {
        selectAllObjects(false);
        for (auto i = 0; i < selectedRows.size(); i++) {
            if (auto object = objects[selectedRows[i]]) {
                object->setSelected(true, false);
            }
        }
    }

    /**
     * @brief Gets the index of a specific object in the container.
     * @param searchObject A shared pointer to the object to search for.
     * @return The index of the object, or -1 if not found.
     */
    int getIndex(std::shared_ptr<const ElementType> searchObject) const {
        auto it = std::find(objects.begin(), objects.end(), searchObject);
        if (it != objects.end())
            return static_cast<int>(std::distance(objects.begin(), it));

        return -1;
    }

    /**
     * @brief Resizes the container to the specified size.
     * @param size The new size of the container.
     */
    void resize(std::size_t size) {
        objects.resize(size);
    }
};

/**
 * @brief Moves an item within a container to a new position.
 * @tparam C The type of the container.
 * @param container The container to modify.
 * @param currentIndex The current index of the item.
 * @param indexOfItemToPlaceBefore The index to place the item before.
 */
template<typename C>
void MoveItemBefore(C& container, size_t currentIndex, size_t indexOfItemToPlaceBefore)
{
    if (currentIndex == indexOfItemToPlaceBefore) return;

    jassert(juce::isPositiveAndBelow((int)currentIndex, (int)container.size()));
    jassert(juce::isPositiveAndBelow((int)indexOfItemToPlaceBefore, (int)container.size() + 1));

    if (currentIndex < indexOfItemToPlaceBefore) {
        std::rotate(container.begin() + currentIndex,
                    container.begin() + currentIndex + 1,
                    container.begin() + indexOfItemToPlaceBefore);
    } else {
        std::rotate(container.begin() + indexOfItemToPlaceBefore,
                    container.begin() + currentIndex,
                    container.begin() + currentIndex + 1);
    }
}

} // namespace audium
