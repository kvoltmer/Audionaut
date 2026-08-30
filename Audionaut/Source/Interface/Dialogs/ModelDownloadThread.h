//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/Separation/DemucsModelStore.h"

/**
 * Downloads the Demucs model behind a modal progress window with a Cancel
 * button. Used on first separation and from the Separation settings tab.
 */
class ModelDownloadThread : public juce::ThreadWithProgressWindow
{
public:
    explicit ModelDownloadThread (audium::DemucsModelStore store_) :
        juce::ThreadWithProgressWindow (TRANS ("Downloading the separation model..."), true, true),
        store (std::move (store_))
    {
    }

    void run() override
    {
        succeeded = store.download ([this] (juce::int64 done, juce::int64 total)
        {
            if (total > 0)
            {
                setProgress (static_cast<double> (done) / static_cast<double> (total));
                setStatusMessage (juce::String (done / (1024 * 1024)) + " / " + juce::String (total / (1024 * 1024)) + " MB");
            }
            else
            {
                setProgress (-1.0);
                setStatusMessage (juce::String (done / (1024 * 1024)) + " MB");
            }

            return ! threadShouldExit();
        }, error);
    }

    /**
     * Runs the download modally. Returns true when the model is in place.
     * On failure (not cancellation) shows the reason.
     */
    static bool downloadModally (audium::DemucsModelStore store, juce::Component* parent)
    {
        ModelDownloadThread thread (std::move (store));

        const auto finished = thread.runThread();

        if (! finished)
            return false;   // cancelled

        if (! thread.succeeded)
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                         TRANS ("Download failed"),
                                                         thread.error,
                                                         parent);
            return false;
        }

        return true;
    }

private:
    audium::DemucsModelStore store;
    juce::String error;
    bool succeeded = false;
};
