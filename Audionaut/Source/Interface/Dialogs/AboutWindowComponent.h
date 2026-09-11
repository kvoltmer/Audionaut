//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <nlohmann/json.hpp>

#include "Application/AudiumApplication.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Engine/Separation/DemucsConfig.h"

using json = nlohmann::json;

// See SBicSegmenter.cpp for the rationale: Essentia is a prebuilt library that
// may not be present/linked in every build, so its availability is auto-detected
// via header presence unless the build system defines ESSENTIA_ENABLED explicitly.
#ifndef ESSENTIA_ENABLED
 #if __has_include(<essentia/algorithmfactory.h>) && __has_include(<unsupported/Eigen/CXX11/Tensor>)
  #define ESSENTIA_ENABLED 1
 #else
  #define ESSENTIA_ENABLED 0
 #endif
#endif

#if ESSENTIA_ENABLED
 #define EIGEN_HAS_STD_RESULT_OF 0
 #include <essentia/essentia.h>
#endif

class AboutWindowComponent final : public Component
{
public:
    AboutWindowComponent()
    {
        // Title - a link to the home page, standing in for the old
        // About Us button
        addAndMakeVisible (titleButton);
        titleButton.setFont (Font (FontOptions (30.0f, Font::FontStyleFlags::bold)),
                             false, Justification::centred);
        titleButton.setTooltip ( {} );

        // Version
        auto buildDate = Time::getCompilationDate();
        auto buildDateStr = String (buildDate.getDayOfMonth()) + " " + Time::getMonthName (buildDate.getMonth(), true) + " " + String (buildDate.getYear());
        addAndMakeVisible (versionLabel);
        versionLabel.setText ("Version: " + AudiumApplication::getApp().getApplicationVersion()
                              + " Build date: " + buildDateStr,
                              dontSendNotification);
        versionLabel.setJustificationType (Justification::centred);
        versionLabel.setColour (Label::backgroundColourId, Colours::transparentBlack);

        // The credits: every row with a home page is a link there, the rest
        // are plain labels. One list drives construction, layout and the
        // preferred height, so rows can come and go (some depend on the
        // backends compiled in) without any of the three drifting.
        auto jsonVersionString = "JSON for Modern C++ v" + String(NLOHMANN_JSON_VERSION_MAJOR) + "."
                                    + String(NLOHMANN_JSON_VERSION_MINOR) + "."
                                    + String(NLOHMANN_JSON_VERSION_PATCH);

        addCredit ("Design by sansculotte.", "https://sansculotte.net/index.php?page=4&lang=1");
        addCredit ("AutoEdit inspired by Oswald Berthold.", "https://github.com/x75");
        addCredit ("Thanks to ELAK Vienna - www.elakwien.at", "https://www.elakwien.at");
        addCreditGap();
        addCreditGap();
        addCredit ("With much gratitude to the following 3rd party libraries:", {});
        addCreditGap();
        addCredit (SystemStats::getJUCEVersion(), "https://juce.com");
        addCredit (jsonVersionString, "https://github.com/nlohmann/json");
        addCredit ("Ableton Link 3.1.2", "https://github.com/Ableton/link");
        addCredit ("FAbian's Realtime Box o' Tricks", "https://github.com/hogliux/farbot");
#if ESSENTIA_ENABLED
        addCredit ("Essentia " + String (essentia::version) + " (Music Technology Group, Universitat Pompeu Fabra)",
                   "https://essentia.upf.edu");
#endif
#if DEMUCS_ENABLED
        addCredit ("demucs.cpp (Sevag Hanssian, MIT)", "https://github.com/sevagh/demucs.cpp");
        addCredit ("Demucs & the htdemucs model (Alexandre Defossez et al., Meta AI Research)",
                   "https://github.com/facebookresearch/demucs");
#endif
        addCredit ("Signalsmith Stretch (Signalsmith Audio, MIT)",
                   "https://signalsmith-audio.co.uk/code/stretch/");

        // Copyright
        addAndMakeVisible (copyrightLabel);
        copyrightLabel.setText(String (CharPointer_UTF8 ("\xc2\xa9")) + String (" 2025 Klaus Voltmer"), dontSendNotification);
        copyrightLabel.setJustificationType (Justification::centred);
        copyrightLabel.setColour (Label::backgroundColourId, Colours::transparentBlack);

    }

    /**
     * The height everything needs at the standard width: the fixed chrome
     * plus the credit rows, which vary with the optional backends compiled
     * into this build - so the window can size itself instead of clipping a
     * longer credits list.
     */
    int getPreferredHeight() const
    {
        return topMargin + titleHeight + versionHeight
               + creditsTopGap + getCreditsHeight() + copyrightGap + copyrightHeight;
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop(topMargin);
        titleButton.setBounds (bounds.removeFromTop (titleHeight));

        versionLabel.setBounds(bounds.removeFromTop(versionHeight));

        bounds.removeFromTop(creditsTopGap);

        for (const auto& row : creditRows)
        {
            if (row != nullptr)
                row->setBounds (bounds.removeFromTop (creditRowHeight));
            else
                bounds.removeFromTop (creditGapHeight);
        }

        copyrightLabel.setBounds (getLocalBounds().removeFromBottom(copyrightHeight));
    }

    void paint (Graphics& g) override
    {
        g.fillAll (findColour (audium::secondaryBackgroundColourId));
    }

private:
    /// A credit row: a link when @p url is given, a plain label otherwise.
    void addCredit (const String& text, const String& url)
    {
        const auto font = FontOptions (AudiumLookAndFeel::defaultFontSize, Font::FontStyleFlags::italic);

        if (url.isNotEmpty())
        {
            auto link = std::make_unique<HyperlinkButton> (text, URL (url));
            link->setFont (Font (font), false, Justification::centred);
            addAndMakeVisible (*link);
            creditRows.push_back (std::move (link));
        }
        else
        {
            auto label = std::make_unique<Label> (String{}, text);
            label->setJustificationType (Justification::centred);
            label->setFont (font);
            label->setColour (Label::backgroundColourId, Colours::transparentBlack);
            addAndMakeVisible (*label);
            creditRows.push_back (std::move (label));
        }
    }

    /// An empty row between credit groups.
    void addCreditGap()
    {
        creditRows.push_back (nullptr);
    }

    int getCreditsHeight() const
    {
        auto height = 0;

        for (const auto& row : creditRows)
            height += row != nullptr ? creditRowHeight : creditGapHeight;

        return height;
    }

    static constexpr int topMargin = 12;
    static constexpr int titleHeight = 40;
    static constexpr int versionHeight = 36;
    static constexpr int creditsTopGap = 12;
    static constexpr int creditRowHeight = 20;
    static constexpr int creditGapHeight = 10;
    static constexpr int copyrightGap = 24;
    static constexpr int copyrightHeight = 52;

    Label   versionLabel { "version" },
            copyrightLabel { "copy" };

    HyperlinkButton titleButton { ProjectInfo::projectName, URL ("https://audionaut.pro") };

    /// The credit rows in display order; nullptr entries are gaps.
    std::vector<std::unique_ptr<Component>> creditRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutWindowComponent)
};

/**
    Shows the AboutWindowComponent as a startup splash screen. Deletes itself
    (see juce::SplashScreen / DeletedAtShutdown) once deleteAfterDelay() is called,
    so callers should treat it as a fire-and-forget raw pointer, not something owned.
*/
class AboutSplashScreen final : public SplashScreen
{
public:
    AboutSplashScreen()
        : SplashScreen (ProjectInfo::projectName, splashWidth, splashHeight, true)
    {
        addAndMakeVisible (aboutComponent);

        // The credits list varies with the backends compiled in; let the
        // content pick the height rather than clipping it.
        centreWithSize (splashWidth, jmax (splashHeight, aboutComponent.getPreferredHeight()));
        aboutComponent.setBounds (getLocalBounds());
    }

    void paint (Graphics&) override
    {
        // AboutWindowComponent paints its own opaque background.
    }

private:
    static constexpr int splashWidth = 600;
    static constexpr int splashHeight = 420;

    AboutWindowComponent aboutComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutSplashScreen)
};
