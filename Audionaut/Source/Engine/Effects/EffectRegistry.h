//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Effect.h"

namespace audium
{

/**
 * The effect types this engine can create, by type id. One per engine,
 * injected where effects are created (the insert chains through the mixer
 * planner, the CLI): no process-wide registry, several engines live in one
 * process. The factory registers the built-ins; a PluginHost registers the
 * plugins it knows and the substitute resolver: a project element whose
 * type id names a plugin this machine lacks may still name one it has in
 * another format (the same plugin as an Audio Unit here, as a VST3 where the
 * project was saved), and the resolver finds that id from the element's
 * "plugin" block.
 */
class EffectRegistry
{
public:
    using Factory = std::function<std::unique_ptr<Effect>()>;

    EffectRegistry() = default;

    /// Registers a type; a second registration of the same id replaces the first.
    void add (EffectTypeInfo type, Factory factory);

    /// Unregisters a type; false when there was none of that id.
    bool remove (const std::string& typeId);

    /// Registers Trim, EQ, Compressor and Delay.
    void addBuiltInEffects();

    /// A stand-in for an insert element whose type this registry lacks: the type id to create instead, "" for none.
    using SubstituteResolver = std::function<std::string (const nlohmann::json& insertElement)>;
    void setSubstituteResolver (SubstituteResolver resolver) { substituteResolver = std::move (resolver); }

    /// The type id a substitute for the element is registered under, "" when there is no resolver or no match.
    std::string resolveSubstitute (const nlohmann::json& insertElement) const;

    /// A new instance of the type, nullptr for an unknown id.
    std::unique_ptr<Effect> create (const std::string& typeId) const;

    bool hasType (const std::string& typeId) const noexcept;

    /// Every registered type, in registration order.
    const std::vector<EffectTypeInfo>& getTypes() const noexcept { return types; }

private:
    std::vector<EffectTypeInfo> types;
    std::vector<Factory> factories;
    SubstituteResolver substituteResolver;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectRegistry)
};

} // namespace audium
