//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Engine/Group/AudioTrackViewState.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Channel/AudioChannel.h"

namespace audium {

int AudioTrackViewState::getTotalHeight() const
{
    int height = 0;
    auto channels = track.getNumAudioTrackChannels();
    for (auto c = 0; c < channels; c++)
    {
        height += track.getChannel(c)->getChannelHeight();
    }
    return height;
}

void AudioTrackViewState::setChannelHeight(int height)
{
    for (auto i = 0; i < track.getNumAudioTrackChannels(); i++)
    {
        track.getChannel(i)->setChannelHeight(height);
    }
}

bool AudioTrackViewState::writeToJson (json& output) const
{
    output["colour"] = trackColour.toString().toStdString();
    if (isMinimized)
        output["minimized"] = isMinimized;

    auto visibleAnalysis = json::array();
    for (auto type : visibleAnalysisTypes)
        visibleAnalysis.push_back(analysisTypeToString(type));
    output["visible_analysis"] = visibleAnalysis;

    return true;
}

bool AudioTrackViewState::readFromJson (json& input)
{
    if (input.contains("minimized"))
        isMinimized = input["minimized"].template get<bool>();

    if (input.contains("visible_analysis"))
    {
        visibleAnalysisTypes.clear();
        for (const auto& element : input["visible_analysis"])
            if (auto type = analysisTypeFromString(element.template get<std::string>()))
                visibleAnalysisTypes.insert(*type);
    }

    if (input.contains("colour"))
        trackColour = juce::Colour::fromString(input["colour"].template get<std::string>());

    return true;
}

} // namespace audium
