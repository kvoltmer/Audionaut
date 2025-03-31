//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Engine/TimeContext.h"
#include "Engine/PlayList/PositionableBase.h"
#include "Engine/Core/DspClipData.h"
#include "Engine/Provider/TempoProvider.h"

namespace audium {

class DspClip : public PositionableBase
{
public:
    DspClip(std::shared_ptr<TempoProvider> tempoProvider, DspClipData data) :
    tempoProvider(tempoProvider),
    dspClipData(data)
    {}
    
    juce::Range<double> getRegionData(audium::TimeContextType context) const override;
    void setRegionData(juce::Range<double> newRegionData, audium::TimeContextType context) override;
    
    double getAbsolutePosition(audium::TimeContextType context) const override;
    void setAbsolutePosition(double position, audium::TimeContextType context) override;
    
private:
    std::shared_ptr<TempoProvider> tempoProvider;
    
public:
    DspClipData dspClipData;
};

} // namespace audium
