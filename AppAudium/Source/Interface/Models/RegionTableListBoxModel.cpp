/*
  ==============================================================================

    RegionTableListBoxModel.cpp
    Created: 7 Jun 2023 2:01:04pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "RegionTableListBoxModel.h"

//==============================================================================
RegionTableListBoxModel::RegionTableListBoxModel(std::shared_ptr<AudioRegionContainer> audioRegionContainer) :
    audioRegionContainer(audioRegionContainer)
{
    
}

RegionTableListBoxModel::~RegionTableListBoxModel()
{
}

int RegionTableListBoxModel::getNumRows()
{
    return audioRegionContainer->getNumRegions();
}

juce::Colour defaultHighlightColourId(0x2340009);
juce::Colour defaultTextColourId(0x2340002);
juce::Colour defaultHighlightedTextColourId(0x234000a);



void RegionTableListBoxModel::paintRowBackground (juce::Graphics& g,
                                 int rowNumber,
                                 int width, int height,
                                 bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll (defaultHighlightColourId);
}

void RegionTableListBoxModel::paintCell (juce::Graphics& g,
                        int rowNumber,
                        int columnId,
                        int width, int height,
                        bool rowIsSelected)
{
    if (const AudioRegion* const r = audioRegionContainer->getRegion(rowNumber))
    {
        juce::String text;

        if (columnId == 1)
            text = r->name;
//        else if (columnId == 2)
//            text = r->originalFilename;
//        else if (columnId == 3)
//            text = File::descriptionOfSizeInBytes ((int64) r->data.getSize());

        if (rowIsSelected)
            g.setColour (juce::Colours::green);// defaultHighlightedTextColourId);
        else
            g.setColour (juce::Colours::red);

        g.setFont (13.0f);
        g.drawText (text, 4, 0, width - 6, height, juce::Justification::centredLeft, true);
    }
    
}
