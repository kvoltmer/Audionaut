
#include <JuceHeader.h>
#include "AudioGroupListBox.h"
#include "Engine/AudioGroup.h"
#include "Engine/AudioGroupContainer.h"
#include "Engine/AudiumEngine.h"
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
}

AudioGroupListBox::~AudioGroupListBox()
{
}

void AudioGroupListBox::filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY)
{
    if ( !filenames.isEmpty())
    {
        
        jassert(File (filenames[0]).existsAsFile());
        auto name = File (filenames[0]).getFileNameWithoutExtension().toStdString();
        
        // create NEW GROUP
        auto group = audiumEngine->getAudioGroupContainer()->createNewAudioGroup(*audiumEngine->getAudioResourceContainer(),
                                                                                 *audiumEngine->getAudioRegionContainer(),
                                                                                 name);

        auto transportPosition = zoomHandler->xToSeconds(mouseX);
        auto channelPosition = 0;
        for (auto i = 0; i < filenames.size(); i++)
        {
            auto url = URL (File (filenames[i]));
            audiumEngine->getAudioResourceContainer()->addAudioResource(url, *audiumEngine, group, channelPosition, transportPosition);
        }
        
        // disabled for now
        //audiumEngine->createDefaultRegionAndPlayList(group);
    }
    
    updateContent();
    
    setColour(TableListBox::backgroundColourId, findColour(audium::secondaryBackgroundColourId));
    repaint();
}

void AudioGroupListBox::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    setColour(TableListBox::backgroundColourId, findColour(audium::secondaryBackgroundColourId).brighter());
    repaint();
}
void AudioGroupListBox::fileDragExit (const juce::StringArray& files)
{
    setColour(TableListBox::backgroundColourId, findColour(audium::secondaryBackgroundColourId));
    repaint();
}
