//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ClipDynamics.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/AudioSources/audium_AudioTransportSource.h"
#include "Engine/AudioSources/AudiumTransportSource.h"

namespace audium {

ClipDynamics::ClipDynamics(PlayListItem& owner_) :
    owner(owner_)
{
}

void ClipDynamics::setGain(int channel, double val, bool continous)
{
    if (channel < 0)
        return;

    while (channel >= static_cast<int>(gains.size())) {
        gains.push_back(1.0);
    }
    gains[static_cast<size_t>(channel)] = val;

    if (continous) {
        for (const auto &source : owner.getTransportSources()) {
            if (source != nullptr &&
                source->getAudioResource().getChannelMapping().getDestinationChannel() == channel &&
                source->isPlaying()) {
                source->getAudioTransportSource()->setGain(static_cast<float>(val));
            }
        }
    }
}

double ClipDynamics::getGain(int channel) const
{
    if (channel >= 0 && channel < static_cast<int>(gains.size())) {
        return gains[static_cast<size_t>(channel)];
    }
    return 1.0;
}

void ClipDynamics::onDeleteChannel(int channel)
{
    if (channel >= 0 && channel < static_cast<int>(gains.size())) {
        gains.erase(gains.begin() + channel);
    }
}

void ClipDynamics::copyGainsFrom(const ClipDynamics& other)
{
    gains = other.gains;
}

double ClipDynamics::getRegionLengthClocks() const
{
    return owner.getRegionData(audium::clocks).getLength();
}

bool ClipDynamics::setFadeIn(double val)
{
    auto length = getRegionLengthClocks();
    fadeInClocks = length * val;
    if (fadeInClocks + fadeOutClocks > length) {
        fadeOutClocks = length - fadeInClocks;
        return true;
    }
    return false;
}

double ClipDynamics::getFadeIn() const
{
    auto length = getRegionLengthClocks();
    if (length > 0.0)
        return fadeInClocks / length;

    return 0.0;
}

bool ClipDynamics::setFadeOut(double val)
{
    auto length = getRegionLengthClocks();
    fadeOutClocks = length * val;
    if (fadeInClocks + fadeOutClocks > length) {
        fadeInClocks = length - fadeOutClocks;
        return true;
    }
    return false;
}

double ClipDynamics::getFadeOut() const
{
    auto length = getRegionLengthClocks();
    if (length > 0.0)
        return fadeOutClocks / length;

    return 0.0;
}

void ClipDynamics::writeToJson(json& output) const
{
    if (fadeInClocks > 0.0) {
        output["fade_in_clocks"]    = fadeInClocks;
    }

    if (fadeOutClocks > 0.0) {
        output["fade_out_clocks"]   = fadeOutClocks;
    }

    // Written whenever non-empty (even all-1.0): an absent key means "never set"
    // and triggers the legacy migration read from the region's gain_vector.
    if (!gains.empty()) {
        output["gain_vector"] = gains;
    }
}

void ClipDynamics::readFromJson(json& input)
{
    if (input.contains("fade_in_clocks")) {
        fadeInClocks = input.at("fade_in_clocks").get<double>();
    }

    if (input.contains("fade_out_clocks")) {
        fadeOutClocks = input.at("fade_out_clocks").get<double>();
    }

    if (input.contains("gain_vector")) {
        input.at("gain_vector").get_to(gains);
    }
    else {
        // Legacy projects stored clip gain on the region; also resets
        // reused item objects when restoring an undo state without gains.
        gains = owner.getRegion()->data.gain_vector;
    }
}

} // namespace audium
