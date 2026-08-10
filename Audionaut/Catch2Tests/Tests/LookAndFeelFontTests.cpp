//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <JuceHeader.h>

#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Interface/Controls/DraggerControl.h"

using namespace juce;

// Inter is embedded in BinaryData rather than installed on the system, so any
// font the platform is asked to resolve by that name comes back null. JUCE
// renames a font to its typeface's family in Font::setTypeface(), which happens
// during glyph fallback, so fonts do arrive named "Inter" at runtime. Returning
// null for those crashed the app inside TextLayout::draw.
SCENARIO ("AudiumLookAndFeel resolves every font to a real typeface", "[interface][lookandfeel][font]")
{
    const ScopedJuceInitialiser_GUI juceInit;

    AudiumLookAndFeel lookAndFeel;

    GIVEN ("the embedded Inter faces")
    {
        THEN ("each style loads")
        {
            for (const auto flags : { Font::plain, Font::bold, Font::italic,
                                      Font::FontStyleFlags (Font::bold | Font::italic) })
            {
                const Font font (FontOptions (13.0f, flags));
                const auto typeface = lookAndFeel.getTypefaceForFont (font);

                REQUIRE (typeface != nullptr);
                REQUIRE (typeface->getName() == "Inter");
            }
        }
    }

    GIVEN ("a font already named after the embedded family")
    {
        // This is the shape of font that reaches getTypefaceForFont() after
        // JUCE has applied a fallback and renamed it to the resolved family.
        THEN ("it still resolves rather than returning null")
        {
            for (const String style : { "Regular", "Bold", "Italic", "Bold Italic" })
            {
                const Font font (FontOptions {}.withName ("Inter")
                                               .withStyle (style)
                                               .withHeight (13.0f));

                const auto typeface = lookAndFeel.getTypefaceForFont (font);

                REQUIRE (typeface != nullptr);
                REQUIRE (typeface->getName() == "Inter");
                REQUIRE (typeface->getStyle() == style);
            }
        }
    }

    GIVEN ("a font naming a family we do not ship")
    {
        THEN ("resolution is delegated without returning null")
        {
            const Font font (FontOptions {}.withName (Font::getDefaultMonospacedFontName())
                                           .withHeight (13.0f));

            REQUIRE (lookAndFeel.getTypefaceForFont (font) != nullptr);
        }
    }
}

// The layout is built from hardcoded pixel insets and row heights that were
// tuned against the platform default sans. JUCE normalises a font's height to
// ascent + descent, so the embedded face occupies the same vertical box, and
// these bounds hold. If a future font swap breaks one of them the symptom is
// silently clipped text, so assert them rather than trusting inspection.
SCENARIO ("UI text fits the hardcoded layout metrics", "[interface][lookandfeel][font]")
{
    const ScopedJuceInitialiser_GUI juceInit;

    AudiumLookAndFeel lookAndFeel;
    LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    GIVEN ("the dragger label")
    {
        THEN ("12pt text clears the 19px dragger strip at its 4px top inset")
        {
            const auto font = DraggerControl::getLabelFont();

            REQUIRE (font.getHeight() == Catch::Approx (DraggerControl::labelFontHeight));
            REQUIRE ((float) DraggerControl::labelTopInset + font.getAscent() + font.getDescent()
                       <= (float) DraggerControl::draggerHeight);
        }
    }

    GIVEN ("a channel strip track name")
    {
        THEN ("a representative name fits the label width")
        {
            // channelsWidth minus the 25px cleared for the minimise button.
            const auto budget = (float) AudiumLookAndFeel::channelsWidth - 25.0f;
            const auto font = DraggerControl::getLabelFont();

            for (const String name : { "TRK-18-epy-jul.wav", "120-funk-1-sec.wav" })
                REQUIRE (GlyphArrangement::getStringWidth (font, name) <= budget);
        }
    }

    GIVEN ("the table header font")
    {
        THEN ("it is derived from the header height and fits the row")
        {
            const auto height = AudiumLookAndFeel::tableHeaderHeight;
            const Font font (AudiumLookAndFeel::getTableHeaderFontOptions (height));

            REQUIRE (font.getHeight() == Catch::Approx ((float) height * 0.5f));
            REQUIRE (font.getAscent() + font.getDescent() <= (float) height);
        }
    }

    GIVEN ("popup menu and alert window rows")
    {
        THEN ("their text fits the fixed row heights")
        {
            REQUIRE (lookAndFeel.getPopupMenuFont().getHeight()
                       <= (float) AudiumLookAndFeel::popupMenuItemHeight);
            REQUIRE (lookAndFeel.getAlertWindowMessageFont().getHeight()
                       <= (float) lookAndFeel.getAlertWindowButtonHeight());
        }
    }

    // The LookAndFeel destructor asserts if it is still the default one.
    LookAndFeel::setDefaultLookAndFeel (nullptr);
}

// Readouts whose digits change while the user watches them use tabular figures.
// Without them Inter's proportional digits shift the surrounding text as values
// tick over: at 13pt a ten-digit string spans 43.7px of ones but 66.5px of eights.
SCENARIO ("live numeric readouts use tabular figures", "[interface][lookandfeel][font]")
{
    const ScopedJuceInitialiser_GUI juceInit;

    AudiumLookAndFeel lookAndFeel;
    LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

    // Any two equal-length digit strings must measure the same.
    const auto digitsAreUniform = [] (const Font& font)
    {
        const auto reference = GlyphArrangement::getStringWidth (font, "1111111111");

        for (const String digits : { "0000000000", "8888888888", "1234567890" })
            if (GlyphArrangement::getStringWidth (font, digits) != Catch::Approx (reference))
                return false;

        return true;
    };

    GIVEN ("the tabular figures helper")
    {
        THEN ("it makes digit advances uniform")
        {
            REQUIRE (digitsAreUniform (Font (AudiumLookAndFeel::withTabularFigures (FontOptions (13.0f)))));
        }

        THEN ("the untreated font is proportional, so the feature is doing real work")
        {
            REQUIRE_FALSE (digitsAreUniform (Font (FontOptions (13.0f))));
        }
    }

    GIVEN ("slider text boxes, which are numeric by definition")
    {
        THEN ("both the named-font and default-size paths get tabular figures")
        {
            for (const String name : { "Tempo Slider Font 13", "unnamed slider" })
            {
                Slider slider (name);
                const std::unique_ptr<Label> label (lookAndFeel.createSliderTextBox (slider));

                REQUIRE (label != nullptr);
                REQUIRE (digitsAreUniform (label->getFont()));
            }
        }
    }

    LookAndFeel::setDefaultLookAndFeel (nullptr);
}
