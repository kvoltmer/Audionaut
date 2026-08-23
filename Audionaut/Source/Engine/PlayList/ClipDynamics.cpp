//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <algorithm>

#include "ClipDynamics.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/AudioSources/ClipTransportSource.h"
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
                source->getClipTransportSource()->setGain(static_cast<float>(val));
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

void ClipDynamics::copyFadeInFrom(const ClipDynamics& other)
{
    auto length = getRegionLengthClocks();

    fadeInClocks = std::min(other.fadeInClocks, length);
    fadeInStartClocks = std::min(other.fadeInStartClocks, fadeInClocks);
    fadeInCurve = other.fadeInCurve;

    // a split piece carries at most one side, but keep the invariants
    if (fadeInClocks + fadeOutClocks > length) {
        fadeOutClocks = length - fadeInClocks;
        if (fadeOutEndClocks > fadeOutClocks) {
            fadeOutEndClocks = fadeOutClocks;
        }
    }
}

void ClipDynamics::copyFadeOutFrom(const ClipDynamics& other)
{
    auto length = getRegionLengthClocks();

    fadeOutClocks = std::min(other.fadeOutClocks, length);
    fadeOutEndClocks = std::min(other.fadeOutEndClocks, fadeOutClocks);
    fadeOutCurve = other.fadeOutCurve;

    if (fadeInClocks + fadeOutClocks > length) {
        fadeInClocks = length - fadeOutClocks;
        if (fadeInStartClocks > fadeInClocks) {
            fadeInStartClocks = fadeInClocks;
        }
    }
}

void ClipDynamics::copyFrom(const ClipDynamics& other)
{
    gains = other.gains;
    fadeInClocks = other.fadeInClocks;
    fadeOutClocks = other.fadeOutClocks;
    fadeInStartClocks = other.fadeInStartClocks;
    fadeOutEndClocks = other.fadeOutEndClocks;
    fadeInCurve = other.fadeInCurve;
    fadeOutCurve = other.fadeOutCurve;
}

double ClipDynamics::getRegionLengthClocks() const
{
    return owner.getRegionData(audium::clocks).getLength();
}

double ClipDynamics::clocksToContext(double clocks, audium::TimeContextType context) const
{
    if (context == audium::clocks) {
        return clocks;
    }
    else if (context == audium::seconds) {
        return owner.getPlayListContainer().getTempoProvider()->clocksToSeconds(clocks);
    }
    jassertfalse;
    return 0.0;
}

double ClipDynamics::getFadeIn(audium::TimeContextType context) const
{
    return clocksToContext(fadeInClocks, context);
}

double ClipDynamics::getFadeOut(audium::TimeContextType context) const
{
    return clocksToContext(fadeOutClocks, context);
}

double ClipDynamics::getFadeInStart(audium::TimeContextType context) const
{
    return clocksToContext(fadeInStartClocks, context);
}

double ClipDynamics::getFadeOutEnd(audium::TimeContextType context) const
{
    return clocksToContext(fadeOutEndClocks, context);
}

void ClipDynamics::setFadeInCurve(double curve)
{
    fadeInCurve = juce::jlimit(minFadeCurve, maxFadeCurve, curve);
}

void ClipDynamics::setFadeOutCurve(double curve)
{
    fadeOutCurve = juce::jlimit(minFadeCurve, maxFadeCurve, curve);
}

bool ClipDynamics::setFadeIn(double val)
{
    auto length = getRegionLengthClocks();
    fadeInClocks = length * val;

    auto changed = false;
    if (fadeInStartClocks > fadeInClocks) {
        fadeInStartClocks = fadeInClocks;
        changed = true;
    }
    if (fadeInClocks + fadeOutClocks > length) {
        fadeOutClocks = length - fadeInClocks;
        changed = true;
        if (fadeOutEndClocks > fadeOutClocks) {
            fadeOutEndClocks = fadeOutClocks;
        }
    }
    return changed;
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

    auto changed = false;
    if (fadeOutEndClocks > fadeOutClocks) {
        fadeOutEndClocks = fadeOutClocks;
        changed = true;
    }
    if (fadeInClocks + fadeOutClocks > length) {
        fadeInClocks = length - fadeOutClocks;
        changed = true;
        if (fadeInStartClocks > fadeInClocks) {
            fadeInStartClocks = fadeInClocks;
        }
    }
    return changed;
}

double ClipDynamics::getFadeOut() const
{
    auto length = getRegionLengthClocks();
    if (length > 0.0)
        return fadeOutClocks / length;

    return 0.0;
}

bool ClipDynamics::setFadeInStart(double val)
{
    auto length = getRegionLengthClocks();
    fadeInStartClocks = length * std::min(val, 1.0);

    auto changed = false;
    if (fadeInStartClocks > fadeInClocks) {
        fadeInClocks = fadeInStartClocks;
        changed = true;
        if (fadeInClocks + fadeOutClocks > length) {
            fadeOutClocks = length - fadeInClocks;
            if (fadeOutEndClocks > fadeOutClocks) {
                fadeOutEndClocks = fadeOutClocks;
            }
        }
    }
    return changed;
}

double ClipDynamics::getFadeInStart() const
{
    auto length = getRegionLengthClocks();
    if (length > 0.0)
        return fadeInStartClocks / length;

    return 0.0;
}

bool ClipDynamics::setFadeOutEnd(double val)
{
    auto length = getRegionLengthClocks();
    fadeOutEndClocks = length * std::min(val, 1.0);

    auto changed = false;
    if (fadeOutEndClocks > fadeOutClocks) {
        fadeOutClocks = fadeOutEndClocks;
        changed = true;
        if (fadeInClocks + fadeOutClocks > length) {
            fadeInClocks = length - fadeOutClocks;
            if (fadeInStartClocks > fadeInClocks) {
                fadeInStartClocks = fadeInClocks;
            }
        }
    }
    return changed;
}

double ClipDynamics::getFadeOutEnd() const
{
    auto length = getRegionLengthClocks();
    if (length > 0.0)
        return fadeOutEndClocks / length;

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

    // != rather than >: negative offsets (fade extending outside the clip)
    // must survive saving and undo snapshots
    if (fadeInStartClocks != 0.0) {
        output["fade_in_start_clocks"]  = fadeInStartClocks;
    }

    if (fadeOutEndClocks != 0.0) {
        output["fade_out_end_clocks"]   = fadeOutEndClocks;
    }

    if (fadeInCurve != defaultFadeCurve) {
        output["fade_in_curve"]         = fadeInCurve;
    }

    if (fadeOutCurve != defaultFadeCurve) {
        output["fade_out_curve"]        = fadeOutCurve;
    }

    // Written whenever non-empty (even all-1.0): an absent key means "never set"
    // and triggers the legacy migration read from the region's gain_vector.
    if (!gains.empty()) {
        output["gain_vector"] = gains;
    }
}

void ClipDynamics::readFromJson(json& input)
{
    // absent keys mean "no fade": reset so reused item objects (undo) don't
    // keep a stale fade
    fadeInClocks = 0.0;
    fadeOutClocks = 0.0;
    fadeInStartClocks = 0.0;
    fadeOutEndClocks = 0.0;
    fadeInCurve = defaultFadeCurve;
    fadeOutCurve = defaultFadeCurve;

    if (input.contains("fade_in_clocks")) {
        fadeInClocks = input.at("fade_in_clocks").get<double>();
    }

    if (input.contains("fade_out_clocks")) {
        fadeOutClocks = input.at("fade_out_clocks").get<double>();
    }

    if (input.contains("fade_in_start_clocks")) {
        fadeInStartClocks = input.at("fade_in_start_clocks").get<double>();
    }

    if (input.contains("fade_out_end_clocks")) {
        fadeOutEndClocks = input.at("fade_out_end_clocks").get<double>();
    }

    if (input.contains("fade_in_curve")) {
        setFadeInCurve(input.at("fade_in_curve").get<double>());
    }

    if (input.contains("fade_out_curve")) {
        setFadeOutCurve(input.at("fade_out_curve").get<double>());
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
