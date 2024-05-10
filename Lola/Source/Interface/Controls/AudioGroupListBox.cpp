
#include <JuceHeader.h>
#include "AudioGroupListBox.h"

#include "Engine/Group/AudioGroup.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Factory/AudioGroupFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/PlayList/PlayListScheduler.h"

#include "Interface/AudiumLookAndFeel.h"
#include "Interface/Handlers/ZoomHandler.h"

using namespace audium;

//==============================================================================
AudioGroupListBox::AudioGroupListBox (std::shared_ptr<AudiumEngine> audiumEngine,
                                      std::shared_ptr<ZoomHandler> zoomHandler) :
    audium::ListBox("AudioGroupListBox", nullptr),
    audiumEngine(audiumEngine),
    zoomHandler(zoomHandler)
{
    // background is transparent
    setColour(ListBox::backgroundColourId, juce::Colours::transparentBlack);
}

AudioGroupListBox::~AudioGroupListBox()
{
}

void AudioGroupListBox::setNewGroupColour(std::shared_ptr<AudioGroup> audioGroup)
{
    if (audioGroup->getColour() == juce::Colours::pink)
    {
        auto newColour = audium::getNewWaveFormColour();
        
        auto numGroups = audiumEngine->getAudioGroupContainer()->getNumItems();
        for (auto i = 0; i < numGroups; i++)
        {
            if(newColour == audiumEngine->getAudioGroupContainer()->getAudioGroup(i)->getColour())
            {
                newColour = audium::getNewWaveFormColour();
            }
        }
        
        audioGroup->setColour(newColour);
    }
}

void AudioGroupListBox::filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY)
{
    if ( !filenames.isEmpty())
    {
        auto action = std::make_unique<audium::UndoableContainerAction>(*audiumEngine->getAudioGroupContainer());
        
        auto audioGroup = audiumEngine->getAudioGroupContainer()->createNewAudioGroup(*audiumEngine->getAudioResourceContainer(),
                                                                                      juce::String());
        setNewGroupColour(audioGroup);
                
        auto position = zoomHandler->xToClocks(mouseX);

        bool arrangementMode = audiumEngine->getPlayListScheduler()->isArrangementMode();

        std::function<void (std::string)> callback = [](std::string error) {
            juce::NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                        "Failed to open File.",
                                                        "Failed to open: " + juce::String(error));
        };
        
        if (audioGroup->addAudioFiles(filenames, position, arrangementMode, callback))
        {
            action->storeNewState();
            audiumEngine->getUndoManager()->perform(action.release(), "File(s) dropped");
            audiumEngine->getUndoManager()->beginNewTransaction();
        }
        else
        {
            audiumEngine->getAudioGroupContainer()->deleteAudioGroup(audioGroup);
        }
    }
    
    
    setColour(ListBox::backgroundColourId, juce::Colours::transparentBlack);
    repaint();
}

void AudioGroupListBox::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    setColour(ListBox::backgroundColourId, findColour(audium::secondaryBackgroundColourId).brighter().withAlpha(0.5f));
    repaint();
}
void AudioGroupListBox::fileDragExit (const juce::StringArray& files)
{
    setColour(ListBox::backgroundColourId, juce::Colours::transparentBlack);
    repaint();
}
