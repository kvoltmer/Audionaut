/*
  ==============================================================================

    AudioExportThread.h
    Created: 31 Oct 2024 9:40:47am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"

class AudioExportThread  : public juce::ThreadWithProgressWindow
{
public:
    AudioExportThread(AudiumEngine &audiumEngine,
                      audium::ExportAudioConfig &config) :
        juce::ThreadWithProgressWindow ("exporting...", true, true),
        audiumEngine(audiumEngine),
        config(config)
    {
    }

    void run()
    {
//        auto thingsToDo = 100;
//        for (int i = 0; i < thingsToDo; ++i)
//        {
//            // must check this as often as possible, because this is
//            // how we know if the user's pressed 'cancel'
//            if (threadShouldExit())
//                break;
//
//            // this will update the progress bar on the dialog box
//            setProgress (i / (double) thingsToDo);
//
//
//            //   ... do the business here...
//        }
        bounceToFile(config);
    }
    
private:
    
    void bounceToFile(audium::ExportAudioConfig &config);

    
    AudiumEngine &audiumEngine;
    audium::ExportAudioConfig &config;
};

