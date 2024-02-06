/*
  ==============================================================================

    SubGroupDraggerControl.h
    Created: 27 Dec 2023 4:52:45pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "DraggerControl.h"
#include "Engine/Group/AudioGroup.h"

class SubGroupDraggerControl : public DraggerControl
{
public:
    
    SubGroupDraggerControl(juce::Component* componentToDrag,
                           std::shared_ptr<AudiumEngine> audiumEngine,
                           std::shared_ptr<AudioSubGroup> audioSubGroup,
                           std::shared_ptr<ZoomHandler> zoomHandler,
                           juce::Colour colour,
                           std::shared_ptr<RegionSelector> regionSelector) :
        DraggerControl(componentToDrag, audiumEngine, zoomHandler, colour, regionSelector),
        audioSubGroup(audioSubGroup)
    {
    }

    ~SubGroupDraggerControl() override
    {
    }
    
    void setRegionDataInSeconds(const juce::Range<double> newRegionData) override
    {
        // undo        
        if (undoableContainerAction == nullptr)
        {
            std::cout << "new audium::UndoableContainerAction" << std::endl;
            
            undoableContainerAction = new audium::UndoableContainerAction(*audioSubGroup);
            //undoableContainerAction = new audium::UndoableContainerAction(*audiumEngine->getAudioGroupContainer());
            undoableContainerAction->storeOldState();
        }
        
        
        const auto audioResources = audioSubGroup->getAudioResources();
        if (audioResources.size() > 0)
        {
            const auto transportPositionInSeconds = audioSubGroup->getAbsolutePosition(audium::seconds);
            auto regionData = audioResources[0]->getRegionData(audium::seconds);
            
            switch (currentDragMode)
            {
                case leftEdge:
                    {
                        // offset in file
                        auto diff = newRegionData.getStart() - transportPositionInSeconds;
                        auto newLength = regionData.getLength() - diff;
                        auto newStart = regionData.getStart() + diff;
                        
                        for (auto res : audioResources)
                        {
                            res->setRegionData(juce::Range<double>(newStart, newStart + newLength), audium::seconds);
                        }
                        audioSubGroup->setAbsolutePosition(newRegionData.getStart(), audium::seconds);
                        repaint();
                    }
                    break;
                case rightEdge:
                    {
                        // duration
                        regionData.setLength(newRegionData.getLength());
                        for (auto res : audioResources)
                        {
                            res->setRegionData(regionData, audium::seconds);
                        }
                    }
                    break;
                case middleEdge:
                    // position in transport
                    audioSubGroup->setAbsolutePosition(newRegionData.getStart(), audium::seconds);
                    break;
                default:
                    break;
            }
        }
    }
    
    
    bool isSelected() const override
    {
        return audioSubGroup->isSelected();
    }
    
    void setSelected(bool bSelected, bool deselectOthers) override
    {
        if (deselectOthers)
            audioSubGroup->getAudioGroup().deselectAllSubGroups();
        audioSubGroup->setSelected(bSelected);
    }
    
    const juce::String getLabelString() const override
    {
        const auto audioResources = audioSubGroup->getAudioResources();
        if (audioResources.size() > 0)
        {
            return audioResources[0]->getFileNameWithoutExtension();
        }
        return "";
    }
    
    bool validateData() override
    {
        const auto audioResources = audioSubGroup->getAudioResources();
        bool result = false;
        for (auto res : audioResources)
        {
            result |= res->validateData();
        }
        
        if (undoableContainerAction != nullptr)
        {
            // Undo: store new state
            undoableContainerAction->storeNewState();
            audiumEngine->getUndoManager()->perform(undoableContainerAction, "Modify Item");
            audiumEngine->getUndoManager()->beginNewTransaction();
            undoableContainerAction = nullptr;
            std::cout << "undoableContainerAction = nullptr" << std::endl;
        }
        
        return result;
    }
    
    bool keyPressed (const KeyPress& key, Component* originatingComponent) override
    {
        if (key.isKeyCode (KeyPress::deleteKey) || key.isKeyCode (KeyPress::backspaceKey))
        {
            audioSubGroup->getAudioGroup().deleteSelectedSubGroups();
            return true;
        }
        
        return false;
    }
    
    
private:
    std::shared_ptr<AudioSubGroup> audioSubGroup;
};
