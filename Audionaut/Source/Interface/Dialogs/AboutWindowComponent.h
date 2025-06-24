//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <nlohmann/json.hpp>

#include "Application/AudiumApplication.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

using json = nlohmann::json;

class AboutWindowComponent final : public Component
{
public:
    AboutWindowComponent()
    {
        // Title
        addAndMakeVisible (titleLabel);
        titleLabel.setJustificationType (Justification::centred);
        titleLabel.setFont (FontOptions (30.0f, Font::FontStyleFlags::bold));

        // Version
        auto buildDate = Time::getCompilationDate();
        auto buildDateStr = String (buildDate.getDayOfMonth()) + " " + Time::getMonthName (buildDate.getMonth(), true) + " " + String (buildDate.getYear());
        addAndMakeVisible (versionLabel);
        versionLabel.setText ("Version " + AudiumApplication::getApp().getApplicationVersion()
                              + "\nBuild date: " + buildDateStr,
                              dontSendNotification);
        versionLabel.setJustificationType (Justification::centred);

        // 3rd party libraries
        addAndMakeVisible (librariesLabel);

        
        auto jsonVersionString = "JSON for Modern C++ v" + String(NLOHMANN_JSON_VERSION_MAJOR) + "."
                                    + String(NLOHMANN_JSON_VERSION_MINOR) + "."
                                    + String(NLOHMANN_JSON_VERSION_PATCH);
        
        auto fabotString = "FAbian's Realtime Box o' Tricks";
        auto linkString = "Ableton Link 3.1.2";
        
        librariesLabel.setText ("Design by sansculotte.\n\n\nWith much gratitude to the following 3rd party libraries:\n\n"
                                   + SystemStats::getJUCEVersion()
                                   + "\n" + jsonVersionString
                                   + "\n" + linkString
                                   + "\n" + fabotString, dontSendNotification);
        
        librariesLabel.setJustificationType (Justification::centred);
        librariesLabel.setFont (FontOptions (AudiumLookAndFeel::defaultFontSize, Font::FontStyleFlags::italic));
    

        // About Us...
        addAndMakeVisible (aboutButton);
        aboutButton.setTooltip ( {} );
        
        // Copyright
        addAndMakeVisible (copyrightLabel);
        copyrightLabel.setText(String (CharPointer_UTF8 ("\xc2\xa9")) + String (" 2025 Klaus Voltmer"), dontSendNotification);
        copyrightLabel.setJustificationType (Justification::centred);

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
        librariesLabel.setBounds(bounds.removeFromTop(120));
        
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
