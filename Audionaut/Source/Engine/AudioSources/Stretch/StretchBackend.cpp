//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "StretchBackend.h"

#include <atomic>

#include "SignalsmithStretchBackend.h"
#include "BungeeStretchBackend.h"
#include "RubberBandStretchBackend.h"
#include "SoundTouchStretchBackend.h"

namespace audium {

namespace {

std::atomic<StretchEngine> selectedEngine { StretchEngine::Signalsmith };

} // namespace

const char* StretchEngines::name (StretchEngine engine)
{
    switch (engine)
    {
        case StretchEngine::Signalsmith: return "signalsmith";
        case StretchEngine::Bungee:      return "bungee";
        case StretchEngine::RubberBand:  return "rubberband";
        case StretchEngine::SoundTouch:  return "soundtouch";
    }
    return "signalsmith";
}

const char* StretchEngines::displayName (StretchEngine engine)
{
    switch (engine)
    {
        case StretchEngine::Signalsmith: return "Signalsmith Stretch";
        case StretchEngine::Bungee:      return "Bungee";
        case StretchEngine::RubberBand:  return "Rubber Band R3";
        case StretchEngine::SoundTouch:  return "SoundTouch";
    }
    return "Signalsmith Stretch";
}

const char* StretchEngines::licence (StretchEngine engine)
{
    switch (engine)
    {
        case StretchEngine::Signalsmith: return "MIT";
        case StretchEngine::Bungee:      return "MPL-2.0";
        case StretchEngine::RubberBand:  return "GPL-2.0-or-later (commercial licence available)";
        case StretchEngine::SoundTouch:  return "LGPL-2.1";
    }
    return "";
}

std::optional<StretchEngine> StretchEngines::fromName (const std::string& wanted)
{
    for (auto engine : { StretchEngine::Signalsmith, StretchEngine::Bungee,
                         StretchEngine::RubberBand, StretchEngine::SoundTouch })
        if (wanted == name (engine))
            return engine;

    return std::nullopt;
}

std::vector<StretchEngine> StretchEngines::available()
{
    std::vector<StretchEngine> engines { StretchEngine::Signalsmith };

#if STRETCH_BUNGEE_ENABLED
    engines.push_back (StretchEngine::Bungee);
#endif
#if STRETCH_RUBBERBAND_ENABLED
    engines.push_back (StretchEngine::RubberBand);
#endif
#if STRETCH_SOUNDTOUCH_ENABLED
    engines.push_back (StretchEngine::SoundTouch);
#endif

    return engines;
}

bool StretchEngines::isAvailable (StretchEngine engine)
{
    for (auto candidate : available())
        if (candidate == engine)
            return true;

    return false;
}

StretchEngine StretchEngines::getSelected()
{
    return selectedEngine.load();
}

bool StretchEngines::setSelected (StretchEngine engine)
{
    if (! isAvailable (engine))
        return false;

    selectedEngine.store (engine);
    return true;
}

std::unique_ptr<StretchBackend> StretchEngines::create (StretchEngine engine)
{
    switch (engine)
    {
#if STRETCH_BUNGEE_ENABLED
        case StretchEngine::Bungee:     return std::make_unique<BungeeStretchBackend>();
#endif
#if STRETCH_RUBBERBAND_ENABLED
        case StretchEngine::RubberBand: return std::make_unique<RubberBandStretchBackend>();
#endif
#if STRETCH_SOUNDTOUCH_ENABLED
        case StretchEngine::SoundTouch: return std::make_unique<SoundTouchStretchBackend>();
#endif
        default: break;
    }

    return std::make_unique<SignalsmithStretchBackend>();
}

} // namespace audium
