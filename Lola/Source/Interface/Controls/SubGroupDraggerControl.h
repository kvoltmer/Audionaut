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
#include "Engine/Group/AudioClip.h"

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
    
    void commitData(const juce::Range<double> newData, audium::TimeContextType context) override
    {
        // undo        
        if (undoableContainerAction == nullptr)
        {
            //std::cout << "new audium::UndoableContainerAction" << std::endl;
            
            undoableContainerAction = new audium::UndoableContainerAction(*audiumEngine->getAudioGroupContainer(), false);
        }
        
        
        const auto audioResources = audioSubGroup->getAudioResources();
        if (audioResources.size() > 0)
        {
            
            switch (currentDragMode)
            {
                case leftEdge:
                    audioSubGroup->getAudioClip()->setAbsoluteStartPosition(newData.getStart(), context);
                    repaint();
                    break;
                case rightEdge:
                    audioSubGroup->getAudioClip()->setLength(newData.getLength(), context);
                    break;
                case middleEdge:
                    // position in transport
                    audioSubGroup->getAudioClip()->setAbsolutePosition(newData.getStart(), context);
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
            audioSubGroup->getAudioGroup().selectAllSubGroups(bSelected);
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
        bool result = audioSubGroup->getAudioClip()->validateData();
        
        if (undoableContainerAction != nullptr)
        {
            // Undo: store new state
            undoableContainerAction->storeNewState();
            audiumEngine->getUndoManager()->perform(undoableContainerAction, "Modify Item");
            audiumEngine->getUndoManager()->beginNewTransaction();
            undoableContainerAction = nullptr;
            //std::cout << "undoableContainerAction = nullptr" << std::endl;
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
