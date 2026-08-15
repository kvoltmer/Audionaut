//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudiumLookAndFeel.h"
#include "Interface/Widgets/audium_ListBox.h"
#include "../JuceLibraryCode/BinaryData.h"

using namespace juce;
using namespace audium;

AudiumLookAndFeel::AudiumLookAndFeel()
{
    setupColours();
    setupTypefaces();
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

void AudiumLookAndFeel::setupTypefaces()
{
    interRegular    = Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf,    BinaryData::InterRegular_ttfSize);
    interBold       = Typeface::createSystemTypefaceFor (BinaryData::InterBold_ttf,       BinaryData::InterBold_ttfSize);
    interItalic     = Typeface::createSystemTypefaceFor (BinaryData::InterItalic_ttf,     BinaryData::InterItalic_ttfSize);
    interBoldItalic = Typeface::createSystemTypefaceFor (BinaryData::InterBoldItalic_ttf, BinaryData::InterBoldItalic_ttfSize);
}

Typeface::Ptr AudiumLookAndFeel::getTypefaceForFont (const Font& font)
{
    const auto name = font.getTypefaceName();

    // Inter is embedded, not installed, so the platform cannot resolve a font
    // that names it and would hand back a null typeface. Fonts reach us under
    // that name because Font::setTypeface() renames a font to its typeface's
    // family, which JUCE does when it applies a glyph fallback.
    const auto isOurs = name == Font::getDefaultSansSerifFontName()
                     || (interRegular != nullptr && name == interRegular->getName());

    if (! isOurs)
        return LookAndFeel_V4::getTypefaceForFont (font);

    // The style arrives as flags on a plain Font, but as a style string on one
    // that has been through a typeface round-trip - honour both.
    const auto style  = font.getTypefaceStyle();
    const auto bold   = font.isBold()   || style.containsIgnoreCase ("Bold");
    const auto italic = font.isItalic() || style.containsIgnoreCase ("Italic");

    const auto face = bold && italic ? interBoldItalic
                    : bold           ? interBold
                    : italic         ? interItalic
                                     : interRegular;

    // Never return null: a caller that dereferences it segfaults rather than
    // falling back to a readable font.
    if (face != nullptr)
        return face;

    return LookAndFeel_V4::getTypefaceForFont (font);
}

void AudiumLookAndFeel::setupColours()
{
    setColourScheme(getDarkAudiumColourScheme());
    
    setColour (backgroundColourId,                   Colours::darkgrey);
    setColour (secondaryBackgroundColourId,          Colours::darkgrey.darker());
    setColour (defaultTextColourId,                  Colour(Colours::lightgrey).brighter());
    
    // Grid
    setColour (gridColourId,                         Colours::yellow.withAlpha(0.75f));
    
    // Mute -> orange-ish
    setColour (muteColourId,                         Colours::orange.withAlpha(0.75f));
    
    // Solo -> yellow-ish
    setColour (soloColourId,                         Colours::yellow.withAlpha(0.75f));
    
    setColour (defaultHighlightColourId,             Colour (Colours::lightgrey));

    setColour(listBoxBackgroundColourId, findColour(secondaryBackgroundColourId).brighter().withAlpha(0.3f));
    
    // Table
    setColour(TableListBox::backgroundColourId, findColour(secondaryBackgroundColourId));
    setColour(TableHeaderComponent::backgroundColourId, findColour(backgroundColourId));
    setColour(TableHeaderComponent::highlightColourId, Colours::transparentBlack);
    setColour(TableHeaderComponent::textColourId, findColour(audium::defaultTextColourId));
    
    // Combo Box
    setColour(ComboBox::backgroundColourId, findColour(secondaryBackgroundColourId));
    setColour(ComboBox::outlineColourId, findColour(backgroundColourId));
    
    
    // AlertWindow
    setColour(AlertWindow::backgroundColourId, findColour(secondaryBackgroundColourId));
    
    // ResizableWindow
    setColour(ResizableWindow::backgroundColourId, findColour(secondaryBackgroundColourId));
    
    // PopupMenu
    setColour(PopupMenu::backgroundColourId, findColour(secondaryBackgroundColourId));
    
    // Button
    setColour(TextButton::buttonColourId, findColour(backgroundColourId));

    // Slider
    setColour(Slider::backgroundColourId, Colours::transparentBlack);
    
    // FileBrowserComponent
//    setColour(FileBrowserComponent::currentPathBoxArrowColourId, Colours::green);
    //setColour(FileBrowserComponent::filenameBoxBackgroundColourId, Colours::red);
    
    // Label
    setColour(Label::backgroundColourId, findColour(secondaryBackgroundColourId));
    setColour(Label::textColourId, findColour(audium::defaultTextColourId));
    
    
}

void AudiumLookAndFeel::drawButtonBackground (Graphics& g,
                                           Button& button,
                                           const Colour& backgroundColour,
                                           bool shouldDrawButtonAsHighlighted,
                                           bool shouldDrawButtonAsDown)
{
    auto cornerSize = 1.0f;
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

        //g.setColour (button.findColour (ComboBox::outlineColourId));
        g.setColour (Colours::white.withAlpha(0.4f));
        g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
    }
}

juce::Font AudiumLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return withDefaultMetrics (FontOptions (defaultFontSize));
}

void AudiumLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                   int, int, int, int, juce::ComboBox& box)
{
    auto cornerSize = box.findParentComponentOfClass<ChoicePropertyComponent>() != nullptr ? 0.0f : 3.0f;
    Rectangle<int> boxBounds (0, 0, width, height);

    g.setColour (box.findColour (ComboBox::backgroundColourId));
    g.fillRoundedRectangle (boxBounds.toFloat(), cornerSize);

    g.setColour (box.findColour (ComboBox::outlineColourId));
    g.drawRoundedRectangle (boxBounds.toFloat().reduced (0.5f, 0.5f), cornerSize, 1.0f);

    auto arrowSize = std::min(15, height);
    
    // arrow zone on the left
    Rectangle<int> arrowZone (1, height/2 - arrowSize/2, arrowSize-2, arrowSize-2);
 
    
    if (box.isMouseOver(true))
    {
        g.setColour (Colour(Colours::grey).withAlpha(0.5f));
        g.fillEllipse(arrowZone.getX(),
                      arrowZone.getY(),
                      arrowZone.getWidth(),
                      arrowZone.getHeight());
    }
    
    
    Path path;
    path.startNewSubPath ((float) arrowZone.getX() + 3.0f, (float) arrowZone.getCentreY() - 2.0f);
    path.lineTo ((float) arrowZone.getCentreX(), (float) arrowZone.getCentreY() + 3.0f);
    path.lineTo ((float) arrowZone.getRight() - 3.0f, (float) arrowZone.getCentreY() - 2.0f);

    g.setColour (box.findColour (ComboBox::arrowColourId).withAlpha ((box.isEnabled() ? 0.9f : 0.2f)));
    g.strokePath (path, PathStrokeType (2.0f));
}


void AudiumLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    
    auto height = box.getHeight();
    
    // arrow zone on the left
    auto arrowSize = std::min(15, height);
    
    label.setBounds (1 + arrowSize,
                     1,
                     box.getWidth() - 2 - arrowSize,
                     box.getHeight() - 2);

    label.setFont (getComboBoxFont (box));
}

Label * AudiumLookAndFeel::createComboBoxTextBox (ComboBox & combo)
{
    Label * label = new Label (String(), String());
    label->setInterceptsMouseClicks (false, false);
    return label;
}

PopupMenu::Options AudiumLookAndFeel::getOptionsForComboBoxPopupMenu (ComboBox& box, Label& label)
{
    return PopupMenu::Options().withItemThatMustBeVisible (box.getSelectedId())
                               .withInitiallySelectedItem (box.getSelectedId())
                               .withStandardItemHeight (popupMenuItemHeight);
}

Font AudiumLookAndFeel::getPopupMenuFont()
{
    return withDefaultMetrics (FontOptions (defaultFontSize));
}


Label* AudiumLookAndFeel::createSliderTextBox (Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);

    // try to extract the font size from the name
    auto fontSize = slider.getName().getTrailingIntValue();
    if (fontSize <= 0)
        fontSize = 11;

    // Slider text boxes are numeric by definition, and the transport ones update
    // during playback, so they all want tabular figures.
    l->setFont (withTabularFigures (juce::FontOptions ((float) fontSize)));
    return l;
}

void AudiumLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos,
                                       float minSliderPos,
                                       float maxSliderPos,
                                       const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    g.fillAll (slider.findColour(Slider::backgroundColourId));

    LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
}

void AudiumLookAndFeel::drawTableHeaderColumn (Graphics& g, TableHeaderComponent& header,
                                            const String& columnName, int columnId,
                                            int width, int height, bool isMouseOver, bool isMouseDown,
                                            int columnFlags)
{
    auto highlightColour = header.findColour (TableHeaderComponent::highlightColourId);

    if (isMouseDown)
        g.fillAll (highlightColour);
    else if (isMouseOver)
        g.fillAll (highlightColour.withMultipliedAlpha (0.625f));

    Rectangle<int> area (width, height);
    area.reduce (4, 0);

    if ((columnFlags & (TableHeaderComponent::sortedForwards | TableHeaderComponent::sortedBackwards)) != 0)
    {
        Path sortArrow;
        sortArrow.addTriangle (0.0f, 0.0f,
                               0.5f, (columnFlags & TableHeaderComponent::sortedForwards) != 0 ? -0.8f : 0.8f,
                               1.0f, 0.0f);

        g.setColour (Colour (0x99000000));
        g.fillPath (sortArrow, sortArrow.getTransformToScaleToFit (area.removeFromRight (height / 2).reduced (2).toFloat(), true));
    }

    g.setColour (header.findColour (TableHeaderComponent::textColourId));
    g.setFont (withDefaultMetrics (getTableHeaderFontOptions (height)));
    g.drawFittedText (columnName, area, Justification::centredLeft, 1, 1.f);
}

int AudiumLookAndFeel::getAlertWindowButtonHeight()    { return 20; }
Font AudiumLookAndFeel::getAlertWindowTitleFont()      { return withDefaultMetrics (FontOptions { 14.0f, Font::bold }); }
Font AudiumLookAndFeel::getAlertWindowMessageFont()    { return withDefaultMetrics (FontOptions { defaultFontSize }); }
Font AudiumLookAndFeel::getAlertWindowFont()           { return withDefaultMetrics (FontOptions { 10.0f }); }

const float AudiumLookAndFeel::defaultFontSize = 13.f;

juce::Colour AudiumLookAndFeel::trackColours[maxTrackColours] = {};
