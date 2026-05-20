//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Application/AudiumApplication.h"
#include "Util/Preferences.h"

class FileBrowserView  : public juce::Component, public juce::DragAndDropContainer, private juce::Timer
{
public:
    FileBrowserView()
    {
        auto root = AudiumApplication::getPreferences().getValue (audium::PreferenceKeys::browserRoot);
        if (root.empty())
            root = File::getSpecialLocation (File::userMusicDirectory).getFullPathName().toStdString();
        
        wildcardFileFilter = std::make_unique<WildcardFileFilter> ("*.*", String(), "All files");
        auto flags = FileBrowserComponent::canSelectFiles | FileBrowserComponent::openMode | FileBrowserComponent::useTreeView | FileBrowserComponent::canSelectMultipleItems;
        fileBrowserComponent = std::make_unique<FileBrowserComponent> (flags,
                                                          File(root),
                                                        wildcardFileFilter.get(),
                                                          nullptr);

        addAndMakeVisible(fileBrowserComponent.get());
        
    
        if (auto tree = getFileTreeComponent()) {
            // enable drag & drop without hacking juce::FileBrowserComponent
            tree->setDragAndDropDescription("FileTreeComponent");
        }
        
        // this is retarded code to restore the openness of the file tree
        // see: https://forum.juce.com/t/filetreecomponent-doesnt-restore-openness-after-refresh/25313
        Timer::callAfterDelay(1000, [this]() {
            // Time will call restoreOpenness 10 times ;(
            startTimer(50);
        });
        

    }

    ~FileBrowserView() override
    {
        auto root = fileBrowserComponent->getRoot().getFullPathName().toStdString();
        AudiumApplication::getPreferences().setValue (audium::PreferenceKeys::browserRoot,
                                                      root);
        
        
        
        if (auto tree = getFileTreeComponent()) {
            auto xmlString = tree->getOpennessState(true)->toString();
            AudiumApplication::getPreferences().setValue (audium::PreferenceKeys::fileTreeState,
                                                          xmlString.toStdString());
        }
    }
    
    void timerCallback() override
    {
        if (numOpennessIterations <= 0)
            stopTimer();
        
        restoreOpenness();
        numOpennessIterations--;
        
    }
    
    void restoreOpenness ()
    {
        if (auto tree = getFileTreeComponent()) {
            auto xmlString = AudiumApplication::getPreferences().getValue (audium::PreferenceKeys::fileTreeState);
            if (not xmlString.empty()) {
                auto xmlState = *XmlDocument::parse(xmlString).get();
                tree->restoreOpennessState(xmlState, true);
            }
        }
    }
    
    juce::FileTreeComponent* getFileTreeComponent() const
    {
        // workaround to access juce::FileBrowserComponent
        for (auto i = 0; i < fileBrowserComponent->getNumChildComponents(); ++i)  {
            if (auto tree = dynamic_cast<juce::FileTreeComponent*> (fileBrowserComponent->getChildComponent(i))) {
                return tree;
            }
        }
        return nullptr;
    }

    void paint (juce::Graphics& g) override
    {
    }

    void resized() override
    {
        if (fileBrowserComponent != nullptr)
            fileBrowserComponent->setBounds(getLocalBounds());
    }

private:
    
    std::unique_ptr<juce::FileBrowserComponent> fileBrowserComponent;
    std::unique_ptr<juce::WildcardFileFilter> wildcardFileFilter;
    
    int numOpennessIterations = 10;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FileBrowserView)
};
