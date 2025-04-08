//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>
#include "AudioResourceView.h"

#include "Engine/AudiumEngine.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Group/AudioClip.h"

double AudioResourceView::getRegionStart(audium::TimeContextType context) const
{
    return audioResource->getAudioSubGroup()->getRegionData(context).getStart();
}
