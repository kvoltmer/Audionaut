//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace audium
{

/**
 * A plugin state saved by one format, for the same plugin in another: what a
 * substitute (PluginHost::findSubstitute) is given when the project was saved
 * where the plugin ran as an Audio Unit and is opened where it is a VST3.
 *
 * An Audio Unit's state is a binary property list. Units that share their
 * code with a VST carry the plugin's own chunk in it - "vstdata", or
 * "jucePluginState" from a JUCE-built unit - and that chunk is what the VST3
 * component takes as its state. Nothing here needs the system's property list
 * code, so it reads the same on every platform.
 */
struct ForeignPluginState
{
    /// Whether the bytes are a binary property list, which is how juce's Audio Unit host saves a state.
    static bool isAudioUnitState (const juce::MemoryBlock& state)
    {
        constexpr std::string_view magic = "bplist00";
        return state.getSize() > magic.size() + trailerSize
               && std::string_view (static_cast<const char*> (state.getData()), magic.size()) == magic;
    }

    /// The plugin's own chunk inside an Audio Unit state; nothing when the unit saved none.
    static std::optional<juce::MemoryBlock> chunkOfAudioUnitState (const juce::MemoryBlock& state)
    {
        if (! isAudioUnitState (state))
            return std::nullopt;

        const PropertyList list (state);
        for (const auto* key : { "vstdata", "jucePluginState" })
            if (auto chunk = list.dataOfTopLevelKey (key); chunk.has_value() && ! chunk->isEmpty())
                return chunk;
        return std::nullopt;
    }

    /// An Audio Unit state as the state juce's VST3 host reads (its "VST3PluginState" xml, the chunk as
    /// the component's state); nothing when the unit's state holds no chunk to hand over.
    static std::optional<juce::MemoryBlock> audioUnitStateForVst3 (const juce::MemoryBlock& state)
    {
        const auto chunk = chunkOfAudioUnitState (state);
        if (! chunk.has_value())
            return std::nullopt;

        juce::XmlElement xml ("VST3PluginState");
        xml.createNewChildElement ("IComponent")->addTextElement (chunk->toBase64Encoding());

        juce::MemoryBlock result;
        juce::AudioProcessor::copyXmlToBinary (xml, result);
        return result;
    }

private:
    static constexpr size_t trailerSize = 32;

    /// The little of the binary property list format a plugin state needs: the dictionary on top, its
    /// string keys and data values. Every read is bounds-checked - the bytes come from a project file.
    class PropertyList
    {
    public:
        explicit PropertyList (const juce::MemoryBlock& block) :
            bytes (static_cast<const uint8_t*> (block.getData())),
            size (block.getSize())
        {
            const auto trailer = size - trailerSize;
            offsetSize = bytes[trailer + 6];
            refSize = bytes[trailer + 7];
            numObjects = bigEndian (trailer + 8, 8).value_or (0);
            topObject = bigEndian (trailer + 16, 8).value_or (0);
            offsetTable = bigEndian (trailer + 24, 8).value_or (0);
        }

        std::optional<juce::MemoryBlock> dataOfTopLevelKey (std::string_view wanted) const
        {
            const auto top = offsetOf (topObject);
            if (! top.has_value() || ! has (*top, 1) || (bytes[*top] >> 4) != 0xD)
                return std::nullopt;

            auto at = *top;
            const auto count = lengthAt (at);   // moves `at` past the marker
            if (! count.has_value() || refSize == 0 || *count > size / refSize)
                return std::nullopt;

            for (uint64_t i = 0; i < *count; ++i)
            {
                const auto key = bigEndian (at + i * refSize, refSize);
                const auto value = bigEndian (at + (*count + i) * refSize, refSize);
                if (! key.has_value() || ! value.has_value())
                    return std::nullopt;
                if (isAsciiString (*key, wanted))
                    return dataOf (*value);
            }
            return std::nullopt;
        }

    private:
        bool has (uint64_t at, uint64_t length) const { return at <= size && length <= size - at; }

        std::optional<uint64_t> bigEndian (uint64_t at, uint64_t length) const
        {
            if (length == 0 || length > 8 || ! has (at, length))
                return std::nullopt;
            uint64_t value = 0;
            for (uint64_t i = 0; i < length; ++i)
                value = (value << 8) | bytes[at + i];
            return value;
        }

        std::optional<uint64_t> offsetOf (uint64_t object) const
        {
            if (object >= numObjects || offsetSize == 0 || object > (size / offsetSize))
                return std::nullopt;
            return bigEndian (offsetTable + object * offsetSize, offsetSize);
        }

        /// The count in an object's marker, or in the integer that follows it; `at` ends on the object's content.
        std::optional<uint64_t> lengthAt (uint64_t& at) const
        {
            if (! has (at, 1))
                return std::nullopt;
            const auto low = static_cast<uint64_t> (bytes[at] & 0x0F);
            ++at;
            if (low != 0x0F)
                return low;

            if (! has (at, 1) || (bytes[at] >> 4) != 0x1)
                return std::nullopt;
            const auto width = uint64_t { 1 } << (bytes[at] & 0x0F);
            const auto length = bigEndian (at + 1, width);
            at += 1 + width;
            return length;
        }

        bool isAsciiString (uint64_t object, std::string_view wanted) const
        {
            auto at = offsetOf (object);
            if (! at.has_value() || ! has (*at, 1) || (bytes[*at] >> 4) != 0x5)
                return false;
            const auto length = lengthAt (*at);
            return length.has_value() && *length == wanted.size() && has (*at, *length)
                   && std::string_view (reinterpret_cast<const char*> (bytes + *at), wanted.size()) == wanted;
        }

        std::optional<juce::MemoryBlock> dataOf (uint64_t object) const
        {
            auto at = offsetOf (object);
            if (! at.has_value() || ! has (*at, 1) || (bytes[*at] >> 4) != 0x4)
                return std::nullopt;
            const auto length = lengthAt (*at);
            if (! length.has_value() || ! has (*at, *length))
                return std::nullopt;
            return juce::MemoryBlock (bytes + *at, static_cast<size_t> (*length));
        }

        const uint8_t* bytes;
        uint64_t size;
        uint64_t offsetSize = 0, refSize = 0, numObjects = 0, topObject = 0, offsetTable = 0;
    };
};

} // namespace audium
