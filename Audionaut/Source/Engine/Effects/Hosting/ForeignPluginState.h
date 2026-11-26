//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace audium
{

/**
 * A plugin state saved by one format, for the same plugin in another: what a
 * substitute (PluginHost::findSubstitute) is given when the project was saved
 * where the plugin ran as an Audio Unit and is opened where it is a VST3, or
 * the other way round.
 *
 * An Audio Unit's state is a binary property list. Units that share their
 * code with a VST carry the plugin's own chunk in it - "vstdata", or
 * "jucePluginState" from a JUCE-built unit - and that chunk is what the VST3
 * component takes as its state. The other way, the component's state goes
 * into a list of the unit's shape. Nothing here needs the system's property
 * list code, so it reads and writes the same on every platform.
 *
 * What does not cross: a VST3's edit controller state has no place in a
 * unit's list, and a unit has none to give, so the VST3 gets its component's
 * state alone and derives the controller's from it, as the format asks of
 * it. A unit that keeps its state in another form has no chunk to hand over.
 *
 * That the chunk is the same in both formats is taken on trust: it holds for
 * plugins built from one code base for both, and nothing in the bytes says
 * so. A plugin handed a chunk that is not its own is expected to refuse it.
 */
struct ForeignPluginState
{
    /// What a plugin is to be given for a state a project saved.
    struct Translation
    {
        enum class Kind
        {
            own,          ///< not another format's state: the plugin reads it as it is
            translated,   ///< another format's: `state` is its chunk in the wrapping this plugin reads
            unreadable    ///< another format's, with nothing in it this plugin could be given
        };

        Kind kind = Kind::own;
        juce::MemoryBlock state;
    };

    /// A saved state for the plugin of the given format ("VST3", "AudioUnit") and identifier; reads the state once.
    static Translation forPlugin (const juce::MemoryBlock& state, const juce::String& formatName, const juce::String& identifier)
    {
        if (formatName == "VST3" && isAudioUnitState (state))
            return translationOf (audioUnitStateForVst3 (state));

        if (formatName == "AudioUnit")
            if (const auto xml = xmlOfVst3State (state))
                return translationOf (vst3StateForAudioUnit (*xml, identifier));

        return {};
    }

    //==============================================================================
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

    //==============================================================================
    /// Whether the bytes are the state juce's VST3 host saves: its "VST3PluginState" xml.
    static bool isVst3State (const juce::MemoryBlock& state)
    {
        const auto xml = xmlOfVst3State (state);
        return xml != nullptr;
    }

    /// The component's state inside a VST3 state - the plugin's own chunk; nothing when there is none.
    static std::optional<juce::MemoryBlock> chunkOfVst3State (const juce::MemoryBlock& state)
    {
        const auto xml = xmlOfVst3State (state);
        return xml != nullptr ? chunkOfVst3State (*xml) : std::nullopt;
    }

    /**
     * A component's state without what a JUCE-built VST3 appends to its
     * plugin's own: eight zero bytes, the wrapper's private data, that data's
     * size and the words "JUCEPrivateData". The same plugin as a unit is
     * handed its state whole, so the appendix has to go first. A state that
     * does not end on it comes back as it is.
     */
    static juce::MemoryBlock withoutJucePrivateData (const juce::MemoryBlock& chunk)
    {
        constexpr std::string_view magic = "JUCEPrivateData";
        constexpr size_t sizeField = 8;
        const auto size = chunk.getSize();
        const auto* bytes = static_cast<const uint8_t*> (chunk.getData());
        if (size < magic.size() + 2 * sizeField
            || std::string_view (static_cast<const char*> (chunk.getData()) + (size - magic.size()), magic.size()) != magic)
            return chunk;

        // the size is little-endian, and is the private data's alone
        uint64_t privateSize = 0;
        for (size_t i = 0; i < sizeField; ++i)
            privateSize |= static_cast<uint64_t> (bytes[size - magic.size() - sizeField + i]) << (8 * i);

        const auto appendix = magic.size() + 2 * sizeField;
        if (privateSize > size - appendix)
            return chunk;   // not what it looked like
        return juce::MemoryBlock (chunk.getData(), size - appendix - static_cast<size_t> (privateSize));
    }

    /**
     * A VST3 state as the state juce's Audio Unit host reads: the property
     * list a unit saves (its class info), the chunk under "vstdata" and under
     * "jucePluginState" - a unit reads the one it knows and ignores the other.
     * A chunk that ends on a JUCE wrapper's private data is a JUCE-built
     * plugin's: it goes under "jucePluginState" only, without that appendix.
     *
     * A unit takes a state only when it names the unit, so the list carries
     * the type, subtype and manufacturer out of the unit's identifier
     * ("AudioUnit:Effects/aufx,subt,manu"). Nothing when the identifier has
     * none or the state holds no chunk.
     */
    static std::optional<juce::MemoryBlock> vst3StateForAudioUnit (const juce::MemoryBlock& state,
                                                                   const juce::String& audioUnitIdentifier)
    {
        const auto xml = xmlOfVst3State (state);
        return xml != nullptr ? vst3StateForAudioUnit (*xml, audioUnitIdentifier) : std::nullopt;
    }

private:
    static constexpr size_t trailerSize = 32;
    static constexpr size_t maxChunkSize = 0x3fffffff;   ///< twice this still fits the list's four-byte offsets

    static Translation translationOf (std::optional<juce::MemoryBlock> translated)
    {
        // a plugin takes a state's size as an int
        if (! translated.has_value() || translated->getSize() > 0x7fffffff)
            return { Translation::Kind::unreadable, {} };
        return { Translation::Kind::translated, std::move (*translated) };
    }

    static std::optional<juce::MemoryBlock> chunkOfVst3State (const juce::XmlElement& xml)
    {
        juce::MemoryBlock chunk;
        if (const auto* component = xml.getChildByName ("IComponent"))
            if (chunk.fromBase64Encoding (component->getAllSubText()) && ! chunk.isEmpty())
                return chunk;
        return std::nullopt;
    }

    static std::optional<juce::MemoryBlock> vst3StateForAudioUnit (const juce::XmlElement& xml, const juce::String& audioUnitIdentifier)
    {
        const auto chunk = chunkOfVst3State (xml);
        const auto codes = codesOfAudioUnit (audioUnitIdentifier);
        if (! chunk.has_value() || ! codes.has_value() || chunk->getSize() > maxChunkSize)
            return std::nullopt;

        const auto pluginsOwn = withoutJucePrivateData (*chunk);
        const auto juceBuilt = pluginsOwn.getSize() != chunk->getSize();

        // the keys and their order are the ones a unit's own class info has
        PropertyListWriter list;
        list.addInteger ("manufacturer", codes->manufacturer);
        list.addData ("data", {});
        if (! juceBuilt)
            list.addData ("vstdata", *chunk);
        list.addData ("jucePluginState", pluginsOwn);
        list.addInteger ("subtype", codes->subtype);
        list.addInteger ("version", 0);
        list.addInteger ("type", codes->type);
        list.addString ("name", "Untitled");
        return list.write();
    }

    static std::unique_ptr<juce::XmlElement> xmlOfVst3State (const juce::MemoryBlock& state)
    {
        if (state.getSize() > 0x7fffffff)
            return nullptr;
        auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));
        if (xml != nullptr && ! xml->hasTagName ("VST3PluginState"))
            return nullptr;
        return xml;
    }

    struct AudioUnitCodes
    {
        uint32_t type = 0, subtype = 0, manufacturer = 0;
    };

    /// The three four-character codes an identifier ends on: "...type,subt,manu". juce writes each of a
    /// code's bytes as one character, so the codes are read by characters: a byte over 127 is two in UTF-8.
    static std::optional<AudioUnitCodes> codesOfAudioUnit (const juce::String& identifier)
    {
        constexpr int length = 14;
        if (identifier.length() < length)
            return std::nullopt;

        const auto codes = identifier.getLastCharacters (length);
        if (codes[4] != ',' || codes[9] != ',')
            return std::nullopt;

        std::array<uint32_t, 3> values {};
        for (size_t field = 0; field < values.size(); ++field)
        {
            for (auto i = 0; i < 4; ++i)
            {
                const auto character = static_cast<uint32_t> (codes[static_cast<int> (5 * field) + i]);
                if (character > 0xff)
                    return std::nullopt;   // not a byte: no code of a unit
                values[field] = (values[field] << 8) | character;
            }
        }
        return AudioUnitCodes { values[0], values[1], values[2] };
    }

    /// Writes the one shape of binary property list a unit's state has: a dictionary of integers,
    /// short strings and data under short ASCII keys.
    class PropertyListWriter
    {
    public:
        void addInteger (std::string_view key, uint32_t value)
        {
            juce::MemoryOutputStream object;
            object.writeByte (0x12);   // an integer of four bytes
            object.writeIntBigEndian (static_cast<int> (value));
            add (key, object);
        }

        void addString (std::string_view key, std::string_view ascii)
        {
            juce::MemoryOutputStream object;
            writeString (object, ascii);
            add (key, object);
        }

        void addData (std::string_view key, const juce::MemoryBlock& data)
        {
            juce::MemoryOutputStream object;
            writeMarker (object, 0x4, data.getSize());
            object.write (data.getData(), data.getSize());
            add (key, object);
        }

        juce::MemoryBlock write() const
        {
            // the objects: the dictionary, then the keys, then the values - a reference is one byte
            const auto count = keys.size();
            jassert (count < 15);
            std::vector<uint32_t> offsets;
            juce::MemoryOutputStream out;
            out.write ("bplist00", 8);

            offsets.push_back (static_cast<uint32_t> (out.getPosition()));
            out.writeByte (static_cast<char> (0xD0 | count));
            for (size_t i = 0; i < 2 * count; ++i)
                out.writeByte (static_cast<char> (1 + i));

            for (const auto* objects : { &keys, &values })
            {
                for (const auto& object : *objects)
                {
                    offsets.push_back (static_cast<uint32_t> (out.getPosition()));
                    out.write (object.getData(), object.getSize());
                }
            }

            const auto offsetTable = static_cast<juce::int64> (out.getPosition());
            for (const auto offset : offsets)
                out.writeIntBigEndian (static_cast<int> (offset));

            // the trailer: six bytes unused, the sizes of an offset and of a reference, then three counts
            out.writeRepeatedByte (0, 6);
            out.writeByte (4);
            out.writeByte (1);
            out.writeInt64BigEndian (static_cast<juce::int64> (offsets.size()));
            out.writeInt64BigEndian (0);
            out.writeInt64BigEndian (offsetTable);

            return juce::MemoryBlock (out.getData(), out.getDataSize());
        }

    private:
        /// An object's first byte, the count in it or - from fifteen on - in an integer after it.
        static void writeMarker (juce::MemoryOutputStream& out, int kind, size_t count)
        {
            if (count < 15)
            {
                out.writeByte (static_cast<char> ((kind << 4) | static_cast<int> (count)));
                return;
            }
            out.writeByte (static_cast<char> ((kind << 4) | 0x0F));
            out.writeByte (0x12);
            out.writeIntBigEndian (static_cast<int> (count));
        }

        static void writeString (juce::MemoryOutputStream& out, std::string_view ascii)
        {
            writeMarker (out, 0x5, ascii.size());
            out.write (ascii.data(), ascii.size());
        }

        void add (std::string_view key, const juce::MemoryOutputStream& value)
        {
            juce::MemoryOutputStream keyObject;
            writeString (keyObject, key);
            keys.emplace_back (keyObject.getData(), keyObject.getDataSize());
            values.emplace_back (value.getData(), value.getDataSize());
        }

        std::vector<juce::MemoryBlock> keys, values;
    };

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
