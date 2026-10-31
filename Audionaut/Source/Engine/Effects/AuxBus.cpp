//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AuxBus.h"

namespace audium
{

void AuxBus::writeToJson (json& output) const
{
    output["id"] = id;
    output["name"] = name;
    output["gain"] = getGain();
    output["balance"] = getBalance();
    output["mute"] = getMute();
    output["solo"] = getSolo();
    inserts.writeToJson (output);
}

void AuxBus::readFromJson (const json& input)
{
    if (! input.is_object())
        return;

    if (input.contains ("name") && input["name"].is_string())
        name = input["name"].get<std::string>();
    if (input.contains ("gain") && input["gain"].is_number())
        setGain (input["gain"].get<float>());
    if (input.contains ("balance") && input["balance"].is_number())
        setBalance (input["balance"].get<float>());
    if (input.contains ("mute") && input["mute"].is_boolean())
        setMute (input["mute"].get<bool>());
    if (input.contains ("solo") && input["solo"].is_boolean())
        setSolo (input["solo"].get<bool>());

    inserts.readFromJson (input);
}

} // namespace audium
