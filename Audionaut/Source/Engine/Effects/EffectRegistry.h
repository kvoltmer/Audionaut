//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Effect.h"

namespace audium
{

/**
 * The effect types this engine can create, by type id. One per engine,
 * injected where effects are created (the insert chains through the mixer
 * planner, the CLI): no process-wide registry, several engines live in one
 * process. The factory registers the built-ins; the hosting phase adds a
 * second source for plugins.
 */
class EffectRegistry
{
public:
    using Factory = std::function<std::unique_ptr<Effect>()>;

    EffectRegistry() = default;

    /// Registers a type; a second registration of the same id replaces the first.
    void add (EffectTypeInfo type, Factory factory);

    /// Registers Trim, EQ, Compressor and Delay.
    void addBuiltInEffects();

    /// A new instance of the type, nullptr for an unknown id.
    std::unique_ptr<Effect> create (const std::string& typeId) const;

    bool hasType (const std::string& typeId) const noexcept;

    /// Every registered type, in registration order.
    const std::vector<EffectTypeInfo>& getTypes() const noexcept { return types; }

private:
    std::vector<EffectTypeInfo> types;
    std::vector<Factory> factories;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectRegistry)
};

} // namespace audium
