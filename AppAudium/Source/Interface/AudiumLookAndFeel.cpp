/*
  ==============================================================================

    AudiumLookAndFeel.cpp
    Created: 7 Jun 2023 3:56:54pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudiumLookAndFeel.h"
#include "Interface/Widgets/audium_ListBox.h"

using namespace juce;
using namespace audium;

AudiumLookAndFeel::AudiumLookAndFeel()
{
    setupColours();
}

AudiumLookAndFeel::~AudiumLookAndFeel()
{
}


LookAndFeel_V4::ColourScheme AudiumLookAndFeel::getDarkAudiumColourScheme()
{
    /// TODO: define and use all colours
    return { 0xff323e44, 0xff263238, 0xff323e44,
             0xff8e989b, 0xffffffff, Colours::grey,
             0xffffffff, 0xff181f22, 0xffffffff };
}

void AudiumLookAndFeel::setupColours()
{
    setColourScheme(getDarkAudiumColourScheme());
    
    setColour (backgroundColourId,                   Colours::darkgrey);
    
    setColour (secondaryBackgroundColourId,          Colours::darkgrey.darker());
    
    setColour (defaultTextColourId,                  Colour(Colours::lightgrey).brighter());
    
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
    
    //g.fillAll (owner->findColour(audium::secondaryBackgroundColourId).brighter().withAlpha(0.3f));

    setColour(listBoxBackgroundColourId, findColour(secondaryBackgroundColourId).brighter().withAlpha(0.3f));
    
    // Table
    setColour(TableListBox::backgroundColourId, findColour(secondaryBackgroundColourId));
    setColour(TableHeaderComponent::backgroundColourId, findColour(backgroundColourId));
    setColour(TableHeaderComponent::textColourId, findColour(audium::defaultTextColourId));
    
}

void AudiumLookAndFeel::drawButtonBackground (Graphics& g,
                                           Button& button,
                                           const Colour& backgroundColour,
                                           bool shouldDrawButtonAsHighlighted,
                                           bool shouldDrawButtonAsDown)
{
    auto cornerSize = 3.0f;
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f, 0.5f);

    auto baseColour = backgroundColour.withMultipliedSaturation (button.hasKeyboardFocus (true) ? 1.3f : 0.9f)
                                      .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f);

    if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
        baseColour = baseColour.contrasting (shouldDrawButtonAsDown ? 0.2f : 0.05f);

    g.setColour (baseColour);

    auto flatOnLeft   = button.isConnectedOnLeft();
    auto flatOnRight  = button.isConnectedOnRight();
    auto flatOnTop    = button.isConnectedOnTop();
    auto flatOnBottom = button.isConnectedOnBottom();

    if (flatOnLeft || flatOnRight || flatOnTop || flatOnBottom)
    {
        Path path;
        path.addRoundedRectangle (bounds.getX(), bounds.getY(),
                                  bounds.getWidth(), bounds.getHeight(),
                                  cornerSize, cornerSize,
                                  ! (flatOnLeft  || flatOnTop),
                                  ! (flatOnRight || flatOnTop),
                                  ! (flatOnLeft  || flatOnBottom),
                                  ! (flatOnRight || flatOnBottom));

        g.fillPath (path);

        g.setColour (button.findColour (ComboBox::outlineColourId));
        g.strokePath (path, PathStrokeType (1.0f));
    }
    else
    {
        g.fillRoundedRectangle (bounds, cornerSize);

        g.setColour (button.findColour (ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
    }
}


