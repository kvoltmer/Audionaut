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
    setColour (panIndicatorColourId,                 Colour (0xffc8c8c8));
    setColour (dividerColourId,                      findColour (secondaryBackgroundColourId).darker (0.3f));
    
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
    else if (isConsoleButtonPressedIn (button))
    {
        // recessed: a darker face, an inner shadow falling from the top edge, a dark rim instead of the light one
        g.setColour (baseColour.darker (0.15f));
        g.fillRoundedRectangle (bounds, cornerSize);
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.3f), bounds.getX(), bounds.getY(),
                                           Colours::transparentBlack, bounds.getX(), bounds.getY() + 3.0f, false));
        g.fillRoundedRectangle (bounds, cornerSize);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
    }
    else
    {
        g.fillRoundedRectangle (bounds, cornerSize);

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

void AudiumLookAndFeel::drawStretchableLayoutResizerBar (juce::Graphics& g, int w, int h, bool isVerticalBar,
                                                         bool isMouseOver, bool isMouseDragging)
{
    const auto active = isMouseOver || isMouseDragging;
    const auto thickness = active ? 3.0f : 1.0f;

    auto line = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (w), static_cast<float> (h));
    line = isVerticalBar ? line.withSizeKeepingCentre (thickness, line.getHeight())
                         : line.withSizeKeepingCentre (line.getWidth(), thickness);

    g.setColour (active ? juce::Colour (audium::toggledOnColour).withAlpha (0.85f)
                        : findColour (audium::backgroundColourId));
    g.fillRect (line);
}

void AudiumLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos,
                                       float minSliderPos,
                                       float maxSliderPos,
                                       const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    g.fillAll (slider.findColour(Slider::backgroundColourId));

    if (style == Slider::LinearVertical)
    {
        // a mixer fader: a recessed slot with a dB scale, and a cap with a gradient, an index line and a shadow
        const auto centreX = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
        const auto top = static_cast<float> (y);
        const auto bottom = static_cast<float> (y + height);
        const auto enabled = slider.isEnabled();
        const auto active = slider.isMouseOverOrDragging() && enabled;

        // slot
        const auto slot = Rectangle<float> (centreX - 2.5f, top, 5.0f, bottom - top);
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (slot, 2.5f);
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (slot.withTrimmedBottom (slot.getHeight() * 0.5f).reduced (1.0f, 0.0f), 2.0f);   // inner shadow at the top
        g.setColour (Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (slot.expanded (0.5f), 3.0f, 1.0f);

        // scale on the left: a tick and the dB figure (without sign, as on a console) at the usual marks
        if (slider.getMinimum() <= 0.0 && slider.getMaximum() >= 0.0) {
            const auto tickColour = findColour (audium::defaultTextColourId);
            g.setFont (withTabularFigures (juce::FontOptions (7.5f)));
            const auto tickRight = slot.getX() - 2.0f;
            auto lastFigureY = -100.0f;   // a figure is drawn only where it does not run into the one above
            for (const auto mark : { 12.0, 6.0, 0.0, -6.0, -12.0, -24.0, -36.0, -48.0, -60.0 }) {
                if (mark > slider.getMaximum() || mark < slider.getMinimum())
                    continue;
                const auto tickY = slider.getPositionOfValue (mark);
                const auto unity = mark == 0.0;
                g.setColour (tickColour.withAlpha (unity ? 0.7f : 0.35f));
                g.fillRect (tickRight - 3.0f, tickY - 0.5f, 3.0f, 1.0f);
                if (tickY - lastFigureY >= 9.0f) {
                    g.drawText (String (static_cast<int> (std::abs (mark))),
                                Rectangle<float> (static_cast<float> (x), tickY - 5.0f, tickRight - 4.0f - static_cast<float> (x), 10.0f),
                                Justification::centredRight, false);
                    lastFigureY = tickY;
                }
            }
        }

        // cap
        const auto capWidth = jmin (12.0f, static_cast<float> (width) - 2.0f);
        const auto capHeight = 25.0f;
        const auto cap = Rectangle<float> (centreX - capWidth * 0.5f, sliderPos - capHeight * 0.5f, capWidth, capHeight);

        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (cap.translated (0.0f, 1.5f).expanded (0.5f), 3.0f);   // shadow

        const auto capTop = enabled ? (active ? Colour (0xffe8e8e8) : Colour (0xffd0d0d0)) : Colour (0xff8a8a8a);
        const auto capBottom = enabled ? (active ? Colour (0xffb0b0b0) : Colour (0xff9a9a9a)) : Colour (0xff6a6a6a);
        g.setGradientFill (ColourGradient (capTop, cap.getX(), cap.getY(), capBottom, cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (cap, 3.0f, 1.0f);

        // the index line, lit from above
        g.setColour (Colours::black.withAlpha (0.7f));
        g.fillRect (cap.getX() + 3.0f, cap.getCentreY() - 0.5f, cap.getWidth() - 6.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.5f));
        g.fillRect (cap.getX() + 3.0f, cap.getCentreY() + 0.5f, cap.getWidth() - 6.0f, 1.0f);

        // grip ridges above and below the index
        g.setColour (Colours::black.withAlpha (0.15f));
        for (const auto offset : { -4.5f, 4.5f })
            g.fillRect (cap.getX() + 5.0f, cap.getCentreY() + offset, cap.getWidth() - 10.0f, 1.0f);
        return;
    }

    LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
}

void AudiumLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float rotaryStartAngle, float rotaryEndAngle, Slider& slider)
{
    const auto enabled = slider.isEnabled();
    const auto& properties = slider.getProperties();
    drawDial (g, Rectangle<float> (static_cast<float> (x), static_cast<float> (y), static_cast<float> (width), static_cast<float> (height)),
              rotaryStartAngle, rotaryEndAngle, sliderPos,
              static_cast<float> (static_cast<double> (properties.getWithDefault (dialArcFromProperty, 0.5))),
              static_cast<float> (static_cast<double> (properties.getWithDefault (dialMarkProperty, 0.5))),
              enabled, slider.isMouseOverOrDragging() && enabled, slider.findColour (audium::panIndicatorColourId));
}

void AudiumLookAndFeel::drawDial (Graphics& g, Rectangle<float> area, float startAngle, float endAngle,
                                  float proportion, float arcFromProportion, float markProportion,
                                  bool enabled, bool active, Colour indicator)
{
    const auto arcThickness = 2.0f;
    const auto arcGap = 2.0f;
    const auto diameter = jmin (area.getWidth(), area.getHeight()) - 2.0f * (arcThickness + arcGap);
    const auto knob = Rectangle<float> (diameter, diameter).withCentre (area.getCentre());
    const auto centre = knob.getCentre();
    const auto radius = diameter * 0.5f;
    const auto arcRadius = radius + arcGap + arcThickness * 0.5f;
    const auto angleAt = [=] (float p) { return startAngle + p * (endAngle - startAngle); };
    const auto angle = angleAt (proportion);
    const auto arcFrom = angleAt (arcFromProportion);

    // the sweep, and the value from its origin to the pointer
    Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::black.withAlpha (0.4f));
    g.strokePath (track, PathStrokeType (arcThickness, PathStrokeType::curved, PathStrokeType::rounded));
    if (std::abs (angle - arcFrom) > 0.001f) {
        Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, jmin (arcFrom, angle), jmax (arcFrom, angle), true);
        g.setColour (indicator.withAlpha (enabled ? 0.5f : 0.25f));
        g.strokePath (value, PathStrokeType (arcThickness, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // shadow (as under the fader's cap) and ring
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (knob.translated (0.0f, 1.5f).expanded (0.5f));
    g.setGradientFill (ColourGradient (enabled ? Colour (0xff9a9a9a) : Colour (0xff6a6a6a), knob.getX(), knob.getY(),
                                       enabled ? Colour (0xff5e5e5e) : Colour (0xff454545), knob.getX(), knob.getBottom(), false));
    g.fillEllipse (knob);

    // the face, lit from above
    const auto face = knob.reduced (radius * 0.18f);
    const auto faceTop = enabled ? (active ? Colour (0xfff4f4f4) : Colour (0xffe8e8e8)) : Colour (0xff8e8e8e);
    const auto faceBottom = enabled ? (active ? Colour (0xffd2d2d2) : Colour (0xffc4c4c4)) : Colour (0xff747474);
    g.setGradientFill (ColourGradient (faceTop, face.getX(), face.getY(), faceBottom, face.getX(), face.getBottom(), false));
    g.fillEllipse (face);
    g.setColour (Colours::black.withAlpha (0.25f));
    g.drawEllipse (face, 0.8f);

    // the pointer, from near the rim towards the centre
    const auto outer = centre.getPointOnCircumference (face.getWidth() * 0.5f - 1.5f, angle);
    const auto inner = centre.getPointOnCircumference (face.getWidth() * 0.5f - 1.5f - jmax (4.0f, radius * 0.45f), angle);
    g.setColour (Colours::black.withAlpha (enabled ? 0.85f : 0.5f));
    g.drawLine (Line<float> (inner, outer), jmax (1.5f, radius * 0.14f));

    // the mark, across the track
    if (markProportion >= 0.0f) {
        const auto mark = centre.getPointOnCircumference (arcRadius, angleAt (markProportion));
        const auto along = angleAt (markProportion);   // the mark lies radially: rotate a small bar to the angle
        g.setColour (indicator.withAlpha (enabled ? 1.0f : 0.5f));
        Path bar;
        bar.addRoundedRectangle (-0.75f, -2.0f, 1.5f, 4.0f, 0.5f);
        g.fillPath (bar, AffineTransform::rotation (along).translated (mark.x, mark.y));
    }
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
