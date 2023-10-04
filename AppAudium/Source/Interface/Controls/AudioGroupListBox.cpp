
#include <JuceHeader.h>
#include "AudioGroupListBox.h"
#include "Engine/AudioResourceGroup.h"
#include "Engine/AudiumEngine.h"
#include "Interface/AudiumLookAndFeel.h"

using namespace audium;

//==============================================================================
AudioGroupListBox::AudioGroupListBox (std::shared_ptr<AudiumEngine> audiumEngine,
                                      const juce::String& componentName,
                                      audium::ListBoxModel* model) :
    audiumEngine(audiumEngine),
    audium::ListBox(componentName, model)
{
}

AudioGroupListBox::~AudioGroupListBox()
{
}

void AudioGroupListBox::filesDropped (const juce::StringArray& filenames, int mouseX, int mouseY)
{
    if ( !filenames.isEmpty())
    {
        // create new group
        jassert(File (filenames[0]).existsAsFile());
        auto name = File (filenames[0]).getFileNameWithoutExtension().toStdString();
        auto group = std::shared_ptr<AudioResourceGroup> (new AudioResourceGroup(*audiumEngine->getAudioResourceContainer(), name));
        
        for (auto i = 0; i < filenames.size(); i++)
        {
            auto url = URL (File (filenames[i]));
            audiumEngine->getAudioResourceContainer()->addAudioResource(url, group);
        }
        audiumEngine->createDefaultRegionAndPlayList();
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
