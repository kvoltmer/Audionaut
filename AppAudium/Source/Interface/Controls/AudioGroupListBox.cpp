
#include <JuceHeader.h>
#include "AudioGroupListBox.h"

#include "Engine/Group/AudioGroup.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Factory/AudioGroupFactory.h"
#include "Engine/AudioResourceContainer.h"

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
        
        //jassert(File (filenames[0]).existsAsFile());
        
        juce::String name;
        // create NEW GROUP
        auto group = audiumEngine->getAudioGroupContainer()->createNewAudioGroup(*audiumEngine->getAudioResourceContainer(),
                                                                                 *audiumEngine->getAudioRegionContainer(),
                                                                                 name);
        auto subGroup = group->createNewAudioSubGroup();


        auto transportPosition = zoomHandler->xToSeconds(mouseX);
        for (auto i = 0; i < filenames.size(); i++)
        {
            auto channelPosition = group->getNumChannels();
            auto url = URL (File (filenames[i]));
            audiumEngine->getAudioResourceContainer()->addAudioResource(url,
                                                                        group,
                                                                        subGroup,
                                                                        channelPosition,
                                                                        transportPosition);
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
