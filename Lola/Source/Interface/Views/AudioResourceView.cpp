/*
  ==============================================================================

    AudioResourceView.cpp
    Created: 27 Nov 2023 3:58:42pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "AudioResourceView.h"

#include "Engine/AudiumEngine.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Group/AudioClip.h"

double AudioResourceView::getRegionStart(audium::TimeContextType context) const
{
    return audioResource->getAudioSubGroup()->getRegionData(context).getStart();
}
