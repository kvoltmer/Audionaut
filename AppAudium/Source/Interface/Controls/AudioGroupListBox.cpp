
#include <JuceHeader.h>
#include "AudioGroupListBox.h"
#include "Engine/AudioGroup.h"
#include "Engine/AudioGroupContainer.h"
#include "Engine/AudiumEngine.h"
#include "Interface/AudiumLookAndFeel.h"

using namespace audium;

//==============================================================================
AudioGroupListBox::AudioGroupListBox (std::shared_ptr<AudiumEngine> audiumEngine,
                                      const juce::String& componentName,
                                      audium::ListBoxModel* model) :
    audium::ListBox(componentName, model),
    audiumEngine(audiumEngine)
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
        auto group = audiumEngine->getAudioGroupContainer()->createNewAudioGroup(*audiumEngine->getAudioResourceContainer(),
                                                                                         *audiumEngine->getAudioRegionContainer(),
                                                                                         name);

        for (auto i = 0; i < filenames.size(); i++)
        {
            auto url = URL (File (filenames[i]));
            audiumEngine->getAudioResourceContainer()->addAudioResource(url, *audiumEngine, group);
        }
        audiumEngine->createDefaultRegionAndPlayList(group);
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
