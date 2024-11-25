
#include <JuceHeader.h>
#include "AudioTrackListBox.h"

#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/PlayList/PlayListScheduler.h"

#include "Interface/Handlers/SnapToGridHandler.h"
#include "Interface/AudiumLookAndFeel.h"
#include "Interface/Handlers/ZoomHandler.h"

using namespace audium;

//==============================================================================
AudioTrackListBox::AudioTrackListBox (std::shared_ptr<AudiumEngine> audiumEngine,
                                      std::shared_ptr<ZoomHandler> zoomHandler) :
    audium::ListBox("AudioTrackListBox", nullptr),
    audiumEngine(audiumEngine),
    zoomHandler(zoomHandler)
{
    // background is transparent
    setColour(ListBox::backgroundColourId, juce::Colours::transparentBlack);
}

AudioTrackListBox::~AudioTrackListBox()
{
}

void AudioTrackListBox::setNewGroupColour(std::shared_ptr<AudioTrack> audioTrack)
{
    if (audioTrack->getColour() == juce::Colours::pink)
    {
        auto newColour = audium::getNewWaveFormColour();
        
        auto numGroups = audiumEngine->getAudioTrackContainer()->getNumItems();
        for (auto i = 0; i < numGroups; i++)
        {
            if(newColour == audiumEngine->getAudioTrackContainer()->getAudioTrack(i)->getColour())
            {
                newColour = audium::getNewWaveFormColour();
            }
        }
        
        audioTrack->setColour(newColour);
    }
}

void AudioTrackListBox::filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY)
{
    if ( !filenames.isEmpty())
    {
        auto action = std::make_unique<audium::UndoableContainerAction>(*audiumEngine->getAudioTrackContainer());
        
        auto audioTrack = audiumEngine->getAudioTrackContainer()->createNewAudioTrack(juce::String());
        setNewGroupColour(audioTrack);
                
        auto position = zoomHandler->xToClocks(mouseX);
        zoomHandler->snapToGrid(position);
        
        bool arrangementMode = audiumEngine->getPlayListScheduler()->isArrangementMode();

        std::function<void (std::string)> callback = [](std::string error) {
            juce::NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                        "Failed to open File.",
                                                        "Failed to open: " + juce::String(error));
        };
        
        if (audioTrack->addAudioFiles(filenames, position, arrangementMode, callback))
        {
            action->storeNewState();
            audiumEngine->getUndoManager()->perform(action.release(), "File(s) dropped");
            audiumEngine->getUndoManager()->beginNewTransaction();
        }
        else
        {
            audiumEngine->getAudioTrackContainer()->deleteAudioTrack(audioTrack);
        }
    }
    
    
    setColour(ListBox::backgroundColourId, juce::Colours::transparentBlack);
    repaint();
}

void AudioTrackListBox::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    setColour(ListBox::backgroundColourId, findColour(audium::secondaryBackgroundColourId).brighter().withAlpha(0.5f));
    repaint();
}

void AudioTrackListBox::fileDragMove (const StringArray& files, int x, int y)
{
    auto start = zoomHandler->xToClocks(x);
    auto end = start + 0.01;
    Range<double> rangeInClocks(start, end);
    
    zoomHandler->getSnapToGridHandler()->publishRange(rangeInClocks);
}

void AudioTrackListBox::fileDragExit (const juce::StringArray& files)
{
    setColour(ListBox::backgroundColourId, juce::Colours::transparentBlack);
    zoomHandler->getSnapToGridHandler()->clearRange();
    repaint();
}


void AudioTrackListBox::resized()
{
    // call base class
    audium::ListBox::resized();
    
    
//    auto bounds = getLocalBounds();
//    auto contentBounds = getViewport()->getViewedComponent()->getBounds();
//    std::cout << contentBounds.getHeight() << std::endl;
//    std::cout << bounds.getHeight() << std::endl;
}
