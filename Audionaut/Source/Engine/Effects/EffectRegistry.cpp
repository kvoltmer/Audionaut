//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "EffectRegistry.h"

#include "BuiltIn/CompressorEffect.h"
#include "BuiltIn/Eq3Effect.h"
#include "BuiltIn/TrimEffect.h"

namespace audium
{

void EffectRegistry::add (EffectTypeInfo type, Factory factory)
{
    for (size_t i = 0; i < types.size(); ++i)
    {
        if (types[i].id == type.id)
        {
            types[i] = std::move (type);
            factories[i] = std::move (factory);
            return;
        }
    }

    types.push_back (std::move (type));
    factories.push_back (std::move (factory));
}

void EffectRegistry::addBuiltInEffects()
{
    add (TrimEffect::typeInfo(), [] { return std::make_unique<TrimEffect>(); });
    add (Eq3Effect::typeInfo(), [] { return std::make_unique<Eq3Effect>(); });
    add (CompressorEffect::typeInfo(), [] { return std::make_unique<CompressorEffect>(); });
}

std::unique_ptr<Effect> EffectRegistry::create (const std::string& typeId) const
{
    for (size_t i = 0; i < types.size(); ++i)
        if (types[i].id == typeId)
            return factories[i]();
    return nullptr;
}

bool EffectRegistry::hasType (const std::string& typeId) const noexcept
{
    for (const auto& type : types)
        if (type.id == typeId)
            return true;
    return false;
}

} // namespace audium
