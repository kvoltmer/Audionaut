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
    class SeparationThread : public juce::ThreadWithProgressWindow
    {
    public:
        SeparationThread (audium::StemSeparator& separator_, audium::SeparationJob job_) :
            juce::ThreadWithProgressWindow (TRANS ("Separating stems..."), true, true),
            separator (separator_),
            job (std::move (job_))
        {
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
    };

    static void warn (const juce::String& message, juce::Component* parent)
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     TRANS ("Separate Stems"),
                                                     message,
                                                     parent);
    }
};
