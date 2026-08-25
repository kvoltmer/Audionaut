//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <nlohmann/json.hpp>

#include "Application/AudiumApplication.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

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
        // Title
        addAndMakeVisible (titleLabel);
        titleLabel.setJustificationType (Justification::centred);
        titleLabel.setFont (FontOptions (30.0f, Font::FontStyleFlags::bold));
        titleLabel.setColour (Label::backgroundColourId, Colours::transparentBlack);

        // Version
        auto buildDate = Time::getCompilationDate();
        auto buildDateStr = String (buildDate.getDayOfMonth()) + " " + Time::getMonthName (buildDate.getMonth(), true) + " " + String (buildDate.getYear());
        addAndMakeVisible (versionLabel);
        versionLabel.setText ("Version " + AudiumApplication::getApp().getApplicationVersion()
                              + "\nBuild date: " + buildDateStr,
                              dontSendNotification);
        versionLabel.setJustificationType (Justification::centred);
        versionLabel.setColour (Label::backgroundColourId, Colours::transparentBlack);

        // 3rd party libraries
        addAndMakeVisible (librariesLabel);

        
        auto jsonVersionString = "JSON for Modern C++ v" + String(NLOHMANN_JSON_VERSION_MAJOR) + "."
                                    + String(NLOHMANN_JSON_VERSION_MINOR) + "."
                                    + String(NLOHMANN_JSON_VERSION_PATCH);
        
        auto fabotString = "FAbian's Realtime Box o' Tricks";
        auto linkString = "Ableton Link 3.1.2";

        auto librariesText = "Design by sansculotte.\nAutoEdit inspired by Oswald Berthold.\n\n\nWith much gratitude to the following 3rd party libraries:\n\n"
                                   + SystemStats::getJUCEVersion()
                                   + "\n" + jsonVersionString
                                   + "\n" + linkString
                                   + "\n" + fabotString;

#if ESSENTIA_ENABLED
        librariesText += "\nEssentia " + String (essentia::version) + " (Music Technology Group, Universitat Pompeu Fabra)";
#endif

        librariesLabel.setText (librariesText, dontSendNotification);
        
        librariesLabel.setJustificationType (Justification::centred);
        librariesLabel.setFont (FontOptions (AudiumLookAndFeel::defaultFontSize, Font::FontStyleFlags::italic));
        librariesLabel.setColour (Label::backgroundColourId, Colours::transparentBlack);

        // About Us...
        addAndMakeVisible (aboutButton);
        aboutButton.setTooltip ( {} );

        // Copyright
        addAndMakeVisible (copyrightLabel);
        copyrightLabel.setText(String (CharPointer_UTF8 ("\xc2\xa9")) + String (" 2025 Klaus Voltmer"), dontSendNotification);
        copyrightLabel.setJustificationType (Justification::centred);
        copyrightLabel.setColour (Label::backgroundColourId, Colours::transparentBlack);

    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop(20);
        titleLabel.setBounds (bounds.removeFromTop (40));
    
        versionLabel.setBounds(bounds.removeFromTop(40));
        
        auto aboutButtonHeight = 20;
        auto aboutButtonRect = bounds.removeFromTop(20);
        aboutButtonRect.setHeight(aboutButtonHeight);
        aboutButton.setBounds(aboutButtonRect);
        
        bounds.removeFromTop(50);
        librariesLabel.setBounds(bounds.removeFromTop(155));
        
        copyrightLabel.setBounds (getLocalBounds().removeFromBottom(40));
    }

    void paint (Graphics& g) override
    {
        g.fillAll (findColour (audium::secondaryBackgroundColourId));
    }

private:
    Label   titleLabel { "title", ProjectInfo::projectName },
            versionLabel { "version" },
            librariesLabel { "otherVersion" },
            copyrightLabel { "copy" };

    HyperlinkButton aboutButton { "About Us", URL ("https://audionaut.pro") };

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
