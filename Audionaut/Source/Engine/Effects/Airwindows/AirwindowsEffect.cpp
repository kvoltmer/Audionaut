//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AirwindowsEffect.h"

#include "Engine/Effects/EffectRegistry.h"

namespace audium::airwindows
{

std::string typeIdFor (const AirwindowsDescriptor& descriptor)
{
    return "airwindows:" + juce::String (descriptor.name).toLowerCase().toStdString();
}

void addEffects (EffectRegistry& registry)
{
    for (const auto& descriptor : getDescriptors())
    {
        registry.add ({ typeIdFor (descriptor), descriptor.name, descriptor.category, descriptor.description },
                      [&descriptor] { return std::make_unique<AirwindowsEffect> (descriptor); });
    }
}

AirwindowsEffect::AirwindowsEffect (const AirwindowsDescriptor& descriptor_) :
    Effect ({ typeIdFor (descriptor_), descriptor_.name, descriptor_.category, descriptor_.description }, descriptor_.parameters),
    descriptor (descriptor_)
{
}

AirwindowsEffect::~AirwindowsEffect() = default;

void AirwindowsEffect::prepare (double sampleRate, int maxBlockSize, int numChannels_)
{
    numChannels = numChannels_;
    const auto pairs = static_cast<size_t> ((numChannels + 1) / 2);

    live.clear();
    pristine.clear();
    for (size_t i = 0; i < pairs; ++i)
    {
        live.push_back (descriptor.create());
        pristine.push_back (descriptor.create());
        pristine.back()->setSampleRate (static_cast<float> (sampleRate));

        // the live instance starts as a copy of the pristine one, dither seeds included (Airwindows seeds
        // them from rand() in the constructor), so a reset returns it exactly to where it starts here
        descriptor.copyState (*live.back(), *pristine.back());
        applyParameters (*live.back());
    }

    monoRight.assign ((numChannels % 2) != 0 ? static_cast<size_t> (maxBlockSize) : 0, 0.0f);
    takeParameterChange();   // the live instances carry the current values already
}

void AirwindowsEffect::release()
{
    live.clear();
    pristine.clear();
    monoRight.clear();
    numChannels = 0;
}

void AirwindowsEffect::reset() noexcept
{
    for (size_t i = 0; i < live.size(); ++i)
    {
        descriptor.copyState (*live[i], *pristine[i]);
        applyParameters (*live[i]);
    }
}

void AirwindowsEffect::process (juce::dsp::AudioBlock<float>& block) noexcept
{
    if (live.empty())
        return;

    if (takeParameterChange())
        for (auto& instance : live)
            applyParameters (*instance);

    const auto channels = std::min (static_cast<int> (block.getNumChannels()), numChannels);
    const auto numSamples = static_cast<int> (block.getNumSamples());

    for (auto left = 0, pair = 0; left < channels; left += 2, ++pair)
    {
        auto& instance = *live[static_cast<size_t> (pair)];
        auto* l = block.getChannelPointer (static_cast<size_t> (left));

        if (left + 1 < channels)
        {
            // Airwindows reads both inputs of a sample before it writes its outputs, so in place is safe
            float* io[2] { l, block.getChannelPointer (static_cast<size_t> (left + 1)) };
            instance.processReplacing (io, io, numSamples);
        }
        else if (numSamples <= static_cast<int> (monoRight.size()))
        {
            auto* r = monoRight.data();
            std::copy (l, l + numSamples, r);
            float* io[2] { l, r };
            instance.processReplacing (io, io, numSamples);
            for (auto i = 0; i < numSamples; ++i)
                l[i] = 0.5f * (l[i] + r[i]);
        }
    }
}

double AirwindowsEffect::getTailSeconds() const noexcept
{
    return descriptor.tailSeconds;
}

juce::String AirwindowsEffect::getParameterText (int index, float value) const
{
    if (descriptor.display == nullptr || index < 0 || index >= getNumParameters())
        return {};

    char text[kVstMaxParamStrLen + 1] {};
    descriptor.display (index, value, text);
    auto shown = juce::String (text).trim();

    // Airwindows prints every number with four decimals: three at most read better, two at least, and a label follows
    if (shown.containsOnly ("0123456789.-+") && shown.containsAnyOf ("0123456789"))
    {
        shown = juce::String (shown.getDoubleValue(), 3);
        if (shown.endsWithChar ('0'))
            shown = shown.dropLastCharacters (1);
        if (shown == "-0.00")
            shown = "0.00";
    }

    const juce::String label (descriptor.labels != nullptr ? descriptor.labels[index] : "");
    return label.isEmpty() ? shown : shown + " " + label;
}

bool AirwindowsEffect::parseParameterText (int index, const juce::String& text, float& value) const
{
    if (index < 0 || index >= getNumParameters())
        return false;

    // the number alone: a label typed with it is the one the display shows
    const auto number = text.trim().upToFirstOccurrenceOf (" ", false, false);
    if (descriptor.parse != nullptr && descriptor.parse (index, number.toRawUTF8(), value))
    {
        value = clampParameter (index, value);
        return true;
    }
    return false;
}

void AirwindowsEffect::applyParameters (AirwinConsolidatedBase& instance) const noexcept
{
    for (auto i = 0; i < getNumParameters(); ++i)
        instance.setParameter (i, getParameter (i));
}

} // namespace audium::airwindows
