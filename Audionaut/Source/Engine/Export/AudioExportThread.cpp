//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudioExportThread.h"
#include "AudioExporter.h"

namespace audium {

void AudioExportThread::bounce()
{
    AudioExporter (audiumEngine, config).bounce ([this] (double progress) {
        setProgress (progress);
        return ! (threadShouldExit() || ! isThreadRunning());
    });
}

} // namespace audium
