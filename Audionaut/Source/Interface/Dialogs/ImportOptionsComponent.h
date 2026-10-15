//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <optional>

#include <JuceHeader.h>

#include "Engine/Group/AudioTrackContainer.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

/**
 * Lets interface components ask the application to import audio without
 * linking against it (the test target leaves the app shell out):
 * dynamic_cast juce::JUCEApplication::getInstance() to this.
 */
class AudioImporter
{
public:
    virtual ~AudioImporter() = default;

    /** asks for audio files and imports them into the track, or new tracks if null,
        at the position in clocks, or the playhead if none */
    virtual void askUserToImportAudio(std::shared_ptr<audium::AudioTrack> targetTrack = nullptr,
                                      std::optional<double> positionClocks = std::nullopt) = 0;
};

/**
 * The placement option shown inside the Import... file chooser (the macOS
 * panel's accessory view, the custom pane on Windows): how several selected
 * files are laid out. Native choosers that ignore it (zenity / kdialog) leave
 * the last choice in place. Importing into an existing track offers only the
 * two single-track layouts.
 *
 * The panel around it is drawn by the system in its light or dark appearance,
 * not in Audionaut's colours, so the component has its own look and feel
 * that follows that appearance and leaves its background transparent.
 */
class ImportOptionsComponent : public juce::FilePreviewComponent
{
public:
    using Placement = audium::AudioTrackContainer::ImportPlacement;

    ImportOptionsComponent(Placement initialPlacement, bool offerSeparateTracks)
    {
        panelLookAndFeel.setColourScheme(juce::Desktop::getInstance().isDarkModeActive()
                                             ? juce::LookAndFeel_V4::getDarkColourScheme()
                                             : juce::LookAndFeel_V4::getLightColourScheme());
        setLookAndFeel(&panelLookAndFeel);
        setOpaque(false);

        label.setText(TRANS("Multiple files:"), juce::dontSendNotification);
        label.setFont(juce::FontOptions(AudiumLookAndFeel::defaultFontSize, juce::Font::bold));
        label.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(label);

        if (offerSeparateTracks) {
            placementBox.addItem(TRANS("Separate tracks"), idFor(Placement::separateTracks));
            placementBox.addItem(TRANS("One track, stacked channels"), idFor(Placement::stackedChannels));
            placementBox.addItem(TRANS("One track, back to back"), idFor(Placement::backToBack));
        }
        else {
            placementBox.addItem(TRANS("Stacked channels"), idFor(Placement::stackedChannels));
            placementBox.addItem(TRANS("Back to back"), idFor(Placement::backToBack));
            if (initialPlacement == Placement::separateTracks)
                initialPlacement = Placement::stackedChannels;
        }
        placementBox.setSelectedId(idFor(initialPlacement), juce::dontSendNotification);
        addAndMakeVisible(placementBox);

        setSize(420, 44);
    }

    ~ImportOptionsComponent() override
    {
        setLookAndFeel(nullptr);
    }

    Placement getPlacement() const
    {
        switch (placementBox.getSelectedId())
        {
            case 2:  return Placement::stackedChannels;
            case 3:  return Placement::backToBack;
            default: return Placement::separateTracks;
        }
    }

    static juce::String toString(Placement placement)
    {
        switch (placement)
        {
            case Placement::stackedChannels: return "stacked_channels";
            case Placement::backToBack:      return "back_to_back";
            default:                         return "separate_tracks";
        }
    }

    static Placement fromString(const juce::String& text)
    {
        if (text == toString(Placement::stackedChannels)) return Placement::stackedChannels;
        if (text == toString(Placement::backToBack))      return Placement::backToBack;
        return Placement::separateTracks;
    }

    void selectedFileChanged(const juce::File&) override {}

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(8, 8);
        label.setBounds(bounds.removeFromLeft(120));
        bounds.removeFromLeft(8);
        placementBox.setBounds(bounds.removeFromLeft(240));
    }

private:
    static int idFor(Placement placement)
    {
        switch (placement)
        {
            case Placement::stackedChannels: return 2;
            case Placement::backToBack:      return 3;
            default:                         return 1;
        }
    }

    juce::LookAndFeel_V4 panelLookAndFeel;  // declared first: the children reference it until they are gone
    juce::Label label;
    juce::ComboBox placementBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ImportOptionsComponent)
};
