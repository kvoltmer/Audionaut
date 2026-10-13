//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <nlohmann/json.hpp>

namespace audium {

class AudiumEngine;

namespace cli {

/**
 * @brief Walks the engine's containers into a compact JSON summary.
 *
 * Unlike AudiumEngine::writeToJson (the full persistence format), this is a
 * stable, human/agent-oriented digest: tempo, tracks, their clips (position,
 * duration, region and source file) and the loaded audio resources. Reused
 * by `info` and intended for reuse by a future MCP wrapper.
 */
nlohmann::json makeProjectSummary (AudiumEngine& engine);

} // namespace cli
} // namespace audium
