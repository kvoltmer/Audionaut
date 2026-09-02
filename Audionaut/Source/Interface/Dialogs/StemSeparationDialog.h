//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Application/AudiumApplication.h"
#include "Engine/ActionMessages.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Separation/DemucsBackend.h"
#include "Engine/Separation/DemucsModelStore.h"
#include "Engine/Separation/StemSeparator.h"
#include "Interface/Dialogs/ModelDownloadThread.h"
#include "Interface/Dialogs/SeparationSettingsComponent.h"

/**
 * The "Separate Stems..." command: makes sure the model is there (offering
 * the download if not), runs the separation behind a modal progress window
 * and adds the stems to the project.
 *
 * Modal on purpose: the separation renders through the play list scheduler,
 * which must not be playing meanwhile, and the progress window with its
 * Cancel button keeps the transport out of reach - the same arrangement an
 * export uses.
 */
class StemSeparationDialog
{
public:
    void separate (std::shared_ptr<audium::AudiumEngine> engine, juce::Component* parent)
    {
        auto& prefs = AudiumApplication::getPreferences();
        auto store = audium::DemucsModelStore::createDefault();
        const auto threads = SeparationSettingsComponent::readThreads (prefs);

        auto backend = std::make_shared<audium::DemucsBackend> (store.getModelFile(), threads);
        audium::StemSeparator separator (engine, backend);

        audium::SeparationConfig config;
        config.numThreads = threads;
        config.muteSourceTrack = SeparationSettingsComponent::readMuteSource (prefs);

        if (! separator.targetSelectedClip (config))
        {
            warn (TRANS ("Select the clip to separate first."), parent);
            return;
        }

        if (! audium::DemucsBackend::isCompiledIn())
        {
            warn (TRANS ("This build was made without stem separation."), parent);
            return;
        }

        if (! store.isAvailable())
        {
            const auto megabytes = juce::String (store.getModel().expectedBytes / (1024 * 1024));
            const auto message = TRANS ("Stem separation needs the Demucs model, a one-time download of about ")
                                 + megabytes + TRANS (" MB. It is kept in\n")
                                 + store.getDirectory().getFullPathName()
                                 + TRANS ("\n\nDownload it now?");

            if (! juce::NativeMessageBox::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon,
                                                           TRANS ("Separate Stems"),
                                                           message,
                                                           parent))
                return;

            if (! ModelDownloadThread::downloadModally (store, parent))
                return;
        }

        audium::SeparationJob job;
        juce::String error;

        if (! separator.prepare (config, job, error))
        {
            warn (error, parent);
            return;
        }

        SeparationThread thread (separator, job);

        if (! thread.runThread())
        {
            // Cancelled; render() already removed its scratch files.
            return;
        }

        if (! thread.succeeded)
        {
            if (thread.error.isNotEmpty())
                warn (thread.error, parent);
            return;
        }

        std::vector<int> newTrackIds;

        if (! separator.commit (thread.stems, newTrackIds, error))
        {
            warn (error, parent);
            return;
        }

        engine->getAudioTrackContainer()->sendActionMessage (audium::updateAll);
    }

private:
    /**
     * The band, dancing while the model works: four ASCII equalizers, one
     * per stem. Pure entertainment for a minutes-long wait; the real
     * progress lives in the window's bar and status message.
     */
    class StemBandComponent : public juce::Component,
                              private juce::Timer
    {
    public:
        StemBandComponent()
        {
            for (auto& height : heights)
                height = 1 + random.nextInt (rows);

            setSize (380, rows * lineHeight + lineHeight);
            startTimerHz (8);
        }

        void paint (juce::Graphics& g) override
        {
            const auto colour = getLookAndFeel().findColour (juce::AlertWindow::textColourId);

            g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));

            auto area = getLocalBounds();

            // The bars, top row first. Every line is padded to the same
            // column count so centring cannot shift them against each other.
            for (auto row = 0; row < rows; ++row)
            {
                juce::String line;

                for (auto group = 0; group < audium::numStems; ++group)
                {
                    for (auto column = 0; column < groupWidth; ++column)
                    {
                        const auto height = heights[static_cast<size_t> (group * groupWidth + column)];
                        line << (height >= rows - row ? juce::String::charToString (barCharFor (group))
                                                      : juce::String (" "));
                    }

                    if (group < audium::numStems - 1)
                        line << juce::String::repeatedString (" ", gapWidth);
                }

                g.setColour (colour.withAlpha (0.9f));
                g.drawText (line, area.removeFromTop (lineHeight), juce::Justification::centred);
            }

            // The stem names, centred under their bars.
            juce::String labels;

            for (auto group = 0; group < audium::numStems; ++group)
            {
                const auto name = audium::stemDisplayName (audium::stemFromIndex (group)).toUpperCase();
                const auto padding = groupWidth - name.length();
                labels << juce::String::repeatedString (" ", padding / 2) << name
                       << juce::String::repeatedString (" ", padding - padding / 2);

                if (group < audium::numStems - 1)
                    labels << juce::String::repeatedString (" ", gapWidth);
            }

            g.setColour (colour.withAlpha (0.6f));
            g.drawText (labels, area.removeFromTop (lineHeight), juce::Justification::centred);
        }

    private:
        void timerCallback() override
        {
            // A smoothed random walk per bar, with the odd full-height hit.
            for (auto& height : heights)
                height = juce::jlimit (1, rows, height + random.nextInt (3) - 1);

            if (random.nextInt (4) == 0)
                heights[static_cast<size_t> (random.nextInt (static_cast<int> (heights.size())))] = rows;

            repaint();
        }

        static juce::juce_wchar barCharFor (int group)
        {
            switch (audium::stemFromIndex (group))
            {
                case audium::Stem::Drums:  return '#';
                case audium::Stem::Bass:   return '=';
                case audium::Stem::Other:  return '+';
                case audium::Stem::Vocals: return '~';
            }
            return '|';
        }

        static constexpr int rows = 5;
        static constexpr int groupWidth = 8;
        static constexpr int gapWidth = 3;
        static constexpr int lineHeight = 15;

        std::array<int, static_cast<size_t> (audium::numStems* groupWidth)> heights {};
        juce::Random random;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemBandComponent)
    };

    class SeparationThread : public juce::ThreadWithProgressWindow
    {
    public:
        SeparationThread (audium::StemSeparator& separator_, audium::SeparationJob job_) :
            juce::ThreadWithProgressWindow (TRANS ("Separating stems..."), true, true),
            separator (separator_),
            job (std::move (job_))
        {
            if (auto* window = getAlertWindow())
                window->addCustomComponent (&band);
        }

        void run() override
        {
            succeeded = separator.render (job,
                                          [this] (double fraction, const juce::String& message)
                                          {
                                              setProgress (fraction);
                                              setStatusMessage (message);
                                              return ! threadShouldExit();
                                          },
                                          stems, error);
        }

        audium::PendingStems stems;
        juce::String error;
        bool succeeded = false;

    private:
        audium::StemSeparator& separator;
        audium::SeparationJob job;

        // A child of the alert window for display, but owned here: it
        // removes itself from the window when this thread object goes away.
        StemBandComponent band;
    };

    static void warn (const juce::String& message, juce::Component* parent)
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     TRANS ("Separate Stems"),
                                                     message,
                                                     parent);
    }
};
