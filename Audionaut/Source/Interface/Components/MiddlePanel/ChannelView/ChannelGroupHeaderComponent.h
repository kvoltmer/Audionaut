//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/ActionMessages.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Analysis/AnalysisWorker.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Undo/UndoableEdit.h"
#include "Engine/Group/WaveFormColours.h"

#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Interface/Controls/AudiumLabel.h"
#include "Interface/Dialogs/AnalysisSettingsComponent.h"

/**
 * A round button as the combo boxes draw their arrow zone: a disc in their
 * colours, lit grey under the mouse, with a mark and no text. The mark is a
 * chevron for a button that drops a menu down, or a fold triangle that points
 * down while the button is off (open) and right while it is on (folded).
 */
class DiscButton : public juce::Button
{
public:
    enum class Mark { chevron, fold };

    explicit DiscButton (Mark mark_) : juce::Button ("Disc"), mark (mark_) {}

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto disc = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (findColour (juce::ComboBox::backgroundColourId));
        g.fillEllipse (disc);
        if (highlighted || down) {
            g.setColour (juce::Colours::grey.withAlpha (down ? 0.7f : 0.5f));
            g.fillEllipse (disc);
        }
        g.setColour (findColour (juce::ComboBox::outlineColourId));
        g.drawEllipse (disc, 1.0f);

        // the mark, 7 px across: the chevron as drawComboBox strokes it but finer, with a pointed tip; the triangles filled
        const auto centre = disc.getCentre();
        g.setColour (findColour (juce::ComboBox::arrowColourId).withAlpha (isEnabled() ? 0.9f : 0.2f));
        if (mark == Mark::chevron) {
            juce::Path chevron;
            chevron.startNewSubPath (centre.x - 3.5f, centre.y - 2.0f);
            chevron.lineTo (centre.x, centre.y + 2.0f);
            chevron.lineTo (centre.x + 3.5f, centre.y - 2.0f);
            g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
        }
        else {
            juce::Path triangle;
            if (getToggleState())
                triangle.addTriangle (centre.x - 2.0f, centre.y - 3.5f, centre.x - 2.0f, centre.y + 3.5f, centre.x + 2.0f, centre.y);
            else
                triangle.addTriangle (centre.x - 3.5f, centre.y - 2.0f, centre.x + 3.5f, centre.y - 2.0f, centre.x, centre.y + 2.0f);
            g.fillPath (triangle);
        }
    }

private:
    const Mark mark;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiscButton)
};

/**
 * A palette as a menu item: a row of colour swatches, the current colour ringed.
 * A click on a swatch hands its colour to onPick and closes the menu.
 */
class PaletteMenuItem : public juce::PopupMenu::CustomComponent
{
public:
    PaletteMenuItem (std::vector<juce::Colour> colours_, juce::Colour current_, std::function<void (juce::Colour)> onPick_)
        : juce::PopupMenu::CustomComponent (false), colours (std::move (colours_)), current (current_), onPick (std::move (onPick_))
    {
        setRepaintsOnMouseActivity (true);
    }

    void getIdealSize (int& width, int& height) override
    {
        width = margin + static_cast<int> (colours.size()) * (swatch + gap) + margin;
        height = margin + swatch + margin;
    }

    void paint (juce::Graphics& g) override
    {
        const auto hovered = swatchAt (getMouseXYRelative());
        for (size_t i = 0; i < colours.size(); ++i) {
            const auto area = swatchArea (static_cast<int> (i));
            g.setColour (colours[i]);
            g.fillRoundedRectangle (area, 2.0f);
            if (colours[i] == current || static_cast<int> (i) == hovered) {
                g.setColour (findColour (juce::PopupMenu::textColourId).withAlpha (colours[i] == current ? 0.9f : 0.5f));
                g.drawRoundedRectangle (area.expanded (1.0f), 3.0f, 1.0f);
            }
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto index = swatchAt (e.getPosition());
        if (index < 0)
            return;
        // the pick runs once the menu is gone, as a plain item's action does
        juce::MessageManager::callAsync ([pick = onPick, colour = colours[static_cast<size_t> (index)]] { juce::NullCheckedInvocation::invoke (pick, colour); });
        triggerMenuItem();
    }

private:
    static constexpr int swatch = 14, gap = 3, margin = 6;

    juce::Rectangle<float> swatchArea (int index) const
    {
        return juce::Rectangle<int> (margin + index * (swatch + gap), margin, swatch, swatch).toFloat();
    }

    int swatchAt (juce::Point<int> position) const
    {
        for (size_t i = 0; i < colours.size(); ++i)
            if (swatchArea (static_cast<int> (i)).expanded (static_cast<float> (gap) * 0.5f).contains (position.toFloat()))
                return static_cast<int> (i);
        return -1;
    }

    std::vector<juce::Colour> colours;
    juce::Colour current;
    std::function<void (juce::Colour)> onPick;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PaletteMenuItem)
};

class ChannelGroupHeaderComponent : public juce::Component,
                                    public juce::Label::Listener,
                                    public juce::KeyListener,
                                    public juce::DragAndDropTarget
{
public:
    /// The header's columns: the fold button's on the left, the menu button's on the right, the name between.
    static constexpr int foldColumnWidth = 25;
    static constexpr int menuColumnWidth = 22;
    static constexpr int nameWidth = AudiumLookAndFeel::channelsWidth - 2 - foldColumnWidth - menuColumnWidth;

    ChannelGroupHeaderComponent(std::shared_ptr<audium::AudioTrack> audioTrack, std::shared_ptr<audium::AudiumEngine> engine_) :
        audioTrack(audioTrack),
        engine(std::move (engine_))
    {
        audioTrackNameLabel.reset (new AudiumLabel ("track name",
                                             TRANS ("n/a")));
        addAndMakeVisible (audioTrackNameLabel.get());
        
        audioTrackNameLabel->setFont (juce::FontOptions (12.00f));
        audioTrackNameLabel->setJustificationType (juce::Justification::centredLeft);
        audioTrackNameLabel->setEditable (false, true, false);
    
        // Label colours (also see updateFromEngine):
        audioTrackNameLabel->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        audioTrackNameLabel->setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        audioTrackNameLabel->setColour (juce::TextEditor::highlightColourId, juce::Colours::darkgrey);
        audioTrackNameLabel->setMinimumHorizontalScale(1.f); // avoid scaling of text
        audioTrackNameLabel->addListener (this);
        
        
        // the fold arrow: down while open, right while folded
        addAndMakeVisible (minimizeButton);
        minimizeButton.setClickingTogglesState (true);
        minimizeButton.setTooltip (TRANS ("Fold"));
        // 19 is the dragger height. center horizontally -> 19 - 15 = 4 / 2 = 2
        minimizeButton.setBounds (7, 2, 15, 15);
        
        minimizeButton.onClick = [this]() {
            setMinimized (minimizeButton.getToggleState());
        };

        // the track's view menu: waveform size and analysis overlays
        addAndMakeVisible (menuButton);
        menuButton.setTooltip (TRANS ("Size / Analysis"));
        menuButton.onClick = [this] { showMenu(); };
   
        updateFromEngine (audioTrack);
        
        addKeyListener (this);
    }

    ~ChannelGroupHeaderComponent() override
    {
        audioTrack = nullptr;
        removeKeyListener(this);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(1, 1);
        bounds.removeFromLeft (foldColumnWidth);
        menuButton.setBounds (bounds.removeFromRight (menuColumnWidth).withSizeKeepingCentre (15, 15).translated (-1, 0));   // the fold arrow's size, at the right
        audioTrackNameLabel->setBounds(bounds);
    }
    
    void labelTextChanged (juce::Label* labelThatHasChanged) override
    {
        if (labelThatHasChanged == audioTrackNameLabel.get())
        {
            audium::applyAsUndoableEdit(audioTrack->getAudioTrackContainer(), [this]
            {
                audioTrack->setAudioTrackName(audioTrackNameLabel->getText());
                return true;
            },
            "Set audio track name", false);
        }
    }
    
    void updateFromEngine(std::shared_ptr<audium::AudioTrack> newAudioTrack)
    {
        audioTrack = newAudioTrack;
        audioTrackNameLabel->setText (audioTrack->getAudioTrackName(), juce::dontSendNotification);
        audioTrackNameLabel->setColour (juce::Label::textColourId, audioTrack->getViewState().getColour());
        audioTrackNameLabel->setColour (juce::TextEditor::textColourId, audioTrack->getViewState().getColour());
        audioTrackNameLabel->setColour (juce::Label::textWhenEditingColourId, audioTrack->getViewState().getColour());
        
        const auto minimized = audioTrack->getViewState().getMinimized();
        minimizeButton.setToggleState (minimized, dontSendNotification);
        menuButton.setVisible (! minimized);   // size and overlays concern the rows a folded track hides
    }
    
    void paint (juce::Graphics& g) override;
    
    void setMinimized(bool bMinimized)
    {
        audium::applyAsUndoableEdit(audioTrack->getAudioTrackContainer(), [this, bMinimized]
        {
            audioTrack->getViewState().setMinimized(bMinimized);
            return true;
        },
        "minimize audio track", false);
    }
    
    void toggleAnalysisVisibility(audium::AnalysisType analysisType)
    {
        audium::applyAsUndoableEdit(audioTrack->getAudioTrackContainer(), [this, analysisType]
        {
            audioTrack->getViewState().setAnalysisTypeVisible(analysisType, !audioTrack->getViewState().isAnalysisTypeVisible(analysisType));
            return true;
        },
        "toggle analysis visibility", false);

        // refresh the arrangement so the clips redraw with the new visibility
        audioTrack->getAudioTrackContainer().sendActionMessage(audium::updateArrangementAction);
    }

    void setColour (juce::Colour colour)
    {
        audium::applyAsUndoableEdit(audioTrack->getAudioTrackContainer(), [this, colour]
        {
            audioTrack->getViewState().setColour (colour);
            return true;
        },
        "Set track colour", false);   // the restore broadcasts updateAll: every view shows the colour
    }

    void setChannelHeight(int height)
    {
        audium::applyAsUndoableEdit(audioTrack->getAudioTrackContainer(), [this, height]
        {
            jassert(height > 0);
            audioTrack->getViewState().setMinimized(false);
            audioTrack->getViewState().setChannelHeight(height);
            return true;
        },
        "Set audio track height", false);
    }
    
    
    /// Size / Analysis, under the drop-down button.
    void showMenu()
    {
        auto safeThis = Component::SafePointer<ChannelGroupHeaderComponent> (this);

        PopupMenu m;
        m.setLookAndFeel (&getLookAndFeel());
        // the size the channels share is ticked; a track with mixed sizes ticks none
        const auto numChannels = audioTrack->getNumAudioTrackChannels();
        auto currentHeight = numChannels > 0 ? audioTrack->getChannel (0)->getChannelHeight() : 0;
        for (auto i = 1; i < numChannels; ++i)
            if (audioTrack->getChannel (i)->getChannelHeight() != currentHeight)
                currentHeight = 0;

        PopupMenu sizeMenu;
        auto addSize = [&sizeMenu, safeThis, currentHeight] (const String& name, int height) {
            sizeMenu.addItem (name, true, height == currentHeight, [safeThis, height] {
                if (safeThis != nullptr)
                    safeThis->setChannelHeight (height);
            });
        };
        addSize (TRANS ("micro"), AudiumLookAndFeel::SizeIds::micro);
        addSize (TRANS ("small"), AudiumLookAndFeel::SizeIds::small);
        addSize (TRANS ("medium"), AudiumLookAndFeel::SizeIds::medium);
        addSize (TRANS ("large"), AudiumLookAndFeel::SizeIds::large);
        addSize (TRANS ("huge"), AudiumLookAndFeel::SizeIds::huge);
        
        m.addSubMenu("Size", sizeMenu);
        
        // one toggle per analysis the preferences have switched on (the worker's default types mirror
        // them), and per overlay still showing from before, so it can be switched off; ticked while
        // its overlay shows; no submenu when there is none
        PopupMenu showMenu;
        {
            const auto worker = engine->getAudioResourceContainer()->getAnalysisWorker();
            const auto enabled = worker != nullptr ? worker->getDefaultAnalysisTypes() : std::vector<audium::AnalysisType>();
            for (const auto type : audium::AnalysisWorker::canonicalAnalysisTypes()) {
                const auto shown = audioTrack->getViewState().isAnalysisTypeVisible (type);
                if (! shown && std::find (enabled.begin(), enabled.end(), type) == enabled.end())
                    continue;
                showMenu.addItem (AnalysisSettingsComponent::displayName (type), true, shown, [safeThis, type] {
                    if (safeThis != nullptr)
                        safeThis->toggleAnalysisVisibility (type);
                });
            }
        }
        if (showMenu.getNumItems() > 0)
            m.addSubMenu("Analysis", showMenu);

        // the track colour: the two palettes new tracks draw from, a swatch row each
        PopupMenu colourMenu;
        {
            const auto current = audioTrack->getViewState().getColour();
            auto pick = [safeThis] (juce::Colour colour) { if (safeThis != nullptr) safeThis->setColour (colour); };
            auto itemId = 1;
            for (auto& palette : audium::WaveFormColours::getPalettes())
                colourMenu.addCustomItem (itemId++, std::make_unique<PaletteMenuItem> (std::move (palette), current, pick));
        }
        m.addSubMenu("Colour", colourMenu);
        
        m.showMenuAsync (PopupMenu::Options().withStandardItemHeight(AudiumLookAndFeel::popupMenuItemHeight).withTargetComponent (&menuButton));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (!e.mods.isAnyModifierKeyDown() && !isSelected()) {
            audioTrack->getSelectionManager()->deselectAll();
        }
        
        setSelected(e.mods.isCommandDown() ? !isSelected() : true);
    }
    
    bool isSelected() const
    {
        return audioTrack->isSelected();
    }
    
    void setSelected(bool bSelected)
    {
        audioTrack->setSelected(bSelected);

        // updateAll: the arrangement rows and the right panel's selected-track
        // header need to follow, not just the channel view
        audioTrack->getAudioTrackContainer().sendActionMessage(audium::updateAll);
    }

    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override
    {
        if (key.isKeyCode (juce::KeyPress::deleteKey) || key.isKeyCode (juce::KeyPress::backspaceKey)) {
            audioTrack->getAudioTrackContainer().deleteSelectedObjects();
            return true;
        }
        return false;
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (juce::DragAndDropContainer* container = juce::DragAndDropContainer::findParentDragContainerFor(this)) {
            container->startDragging("ChannelGroupHeaderComponent", this);
        }
    }
        
    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) override;
    
    void updateInsertLines(const juce::DragAndDropTarget::SourceDetails &dragSourceDetails);
    
    void hideInsertLines()
    {
        insertBefore = false;
        
        repaint();
    }
    void itemDragEnter (const SourceDetails &dragSourceDetails) override
    {
        updateInsertLines(dragSourceDetails);
    }
    
    void itemDragMove (const SourceDetails &dragSourceDetails) override
    {
        updateInsertLines(dragSourceDetails);
    }
    
    void itemDragExit (const SourceDetails &dragSourceDetails) override
    {
        hideInsertLines();
    }
    
    void itemDropped (const SourceDetails &dragSourceDetails) override;
    
    bool shouldDrawDragImageWhenOver () override
    {
        return true;
    }
    std::shared_ptr<audium::AudioTrack> getAudioTrack() const { return audioTrack; }

private:
    std::shared_ptr<audium::AudioTrack> audioTrack;
    std::shared_ptr<audium::AudiumEngine> engine;
    
    std::unique_ptr<AudiumLabel> audioTrackNameLabel;
    DiscButton minimizeButton { DiscButton::Mark::fold };
    DiscButton menuButton { DiscButton::Mark::chevron };
    
    bool insertBefore = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelGroupHeaderComponent)
};
