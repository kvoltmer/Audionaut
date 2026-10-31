//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <nlohmann/json.hpp>

namespace audium {

class EffectSlot;
class InsertChain;

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

/**
 * One insert as the summary and the insert verbs report it: index, type,
 * name, enabled flag and state. One shape for both, so an agent reads the
 * same object from `info` and from `add-insert`.
 */
nlohmann::json describeInsert (const EffectSlot& slot, int index);

/** A chain as an array of describeInsert(). */
nlohmann::json describeInserts (const InsertChain& chain);

} // namespace cli
} // namespace audium
