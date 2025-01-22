/*
  ==============================================================================

    AboutWindowComponent.h
    Created: 1 Nov 2024 3:47:07pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "Application/AudiumApplication.h"

class AboutWindowComponent final : public Component
{
public:
    AboutWindowComponent()
    {
        addAndMakeVisible (titleLabel);
        titleLabel.setJustificationType (Justification::centred);
        titleLabel.setFont (FontOptions (35.0f, Font::FontStyleFlags::bold));

        auto buildDate = Time::getCompilationDate();
        addAndMakeVisible (versionLabel);
        versionLabel.setText ("Version " + AudiumApplication::getApp().getApplicationVersion()
                              + "\nBuild date: " + String (buildDate.getDayOfMonth())
                                                 + " " + Time::getMonthName (buildDate.getMonth(), true)
                                                 + " " + String (buildDate.getYear()),
                              dontSendNotification);

        versionLabel.setJustificationType (Justification::centred);
        addAndMakeVisible (copyrightLabel);
        copyrightLabel.setJustificationType (Justification::centred);

        addAndMakeVisible (aboutButton);
        aboutButton.setTooltip ( {} );
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromBottom (20);

//        auto leftSlice   = bounds.removeFromLeft (150);
        auto centreSlice = bounds.withTrimmedRight (150);

//        juceLogoBounds = leftSlice.removeFromTop (150).toFloat();
//        juceLogoBounds.setWidth (juceLogoBounds.getWidth() + 100);
//        juceLogoBounds.setHeight (juceLogoBounds.getHeight() + 100);

        auto titleHeight = 40;

        centreSlice.removeFromTop ((centreSlice.getHeight() / 2) - (titleHeight / 2));

        titleLabel.setBounds (centreSlice.removeFromTop (titleHeight));

        centreSlice.removeFromTop (10);
        versionLabel.setBounds (centreSlice.removeFromTop (40));

        centreSlice.removeFromTop (10);
        aboutButton.setBounds (centreSlice.removeFromTop (20));

        copyrightLabel.setBounds (getLocalBounds().removeFromBottom (50));
    }

    void paint (Graphics& g) override
    {
        g.fillAll (findColour (audium::secondaryBackgroundColourId));

//        if (juceLogo != nullptr)
//            juceLogo->drawWithin (g, juceLogoBounds.translated (-75, -75), RectanglePlacement::centred, 1.0);
    }

private:
    Label titleLabel { "title", "Lola" },
          versionLabel { "version" },
          copyrightLabel { "copyright", String (CharPointer_UTF8 ("\xc2\xa9")) + String (" 2024 Voltmer Systems") };

    HyperlinkButton aboutButton { "About Us", URL ("http://www.voltmer-systems.com") };

//    Rectangle<float> juceLogoBounds;
//
//    std::unique_ptr<Drawable> juceLogo { Drawable::createFromImageData (BinaryData::juce_icon_png,
//                                                                        BinaryData::juce_icon_pngSize) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutWindowComponent)
};
