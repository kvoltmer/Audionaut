/*
  ==============================================================================

    AudiumLookAndFeel.cpp
    Created: 7 Jun 2023 3:56:54pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudiumLookAndFeel.h"
#include "ColourIds.h"

using namespace juce;

AudiumLookAndFeel::AudiumLookAndFeel()
{
    setupColours();
}

AudiumLookAndFeel::~AudiumLookAndFeel()
{
}

void AudiumLookAndFeel::setupColours()
{
    setColour (backgroundColourId,                   Colour (0xff323e44));
    setColour (secondaryBackgroundColourId,          Colour (0xff263238));
    setColour (defaultTextColourId,                  Colours::white);
    setColour (widgetTextColourId,                   Colours::white);
    setColour (defaultButtonBackgroundColourId,      Colour (0xffa45c94));
    setColour (secondaryButtonBackgroundColourId,    Colours::black);
    setColour (userButtonBackgroundColourId,         Colour (0xffa45c94));
    setColour (defaultIconColourId,                  Colours::white);
    setColour (treeIconColourId,                     Colour (0xffa9a9a9));
    setColour (defaultHighlightColourId,             Colour (Colours::lightgrey));
    setColour (defaultHighlightedTextColourId,       Colours::black);
    setColour (codeEditorLineNumberColourId,         Colour (0xffaaaaaa));
    setColour (activeTabIconColourId,                Colours::white);
    setColour (inactiveTabBackgroundColourId,        Colour (0xff181f22));
    setColour (inactiveTabIconColourId,              Colour (0xffa9a9a9));
    setColour (contentHeaderBackgroundColourId,      Colours::black);
    setColour (widgetBackgroundColourId,             Colour (0xff495358));
    setColour (secondaryWidgetBackgroundColourId,    Colour (0xff303b41));
}
