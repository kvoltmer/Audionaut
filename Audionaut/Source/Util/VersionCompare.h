//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <array>
#include <JuceHeader.h>

namespace audium {

/// "v1.2.3", "1.2.3", "1.2" or "1" -> {major, minor, patch}, missing
/// components zero. Tolerant of surrounding whitespace and a leading 'v'
/// (GitHub tags carry one, the App Store version does not).
inline std::array<int, 3> parseVersion (const juce::String& text)
{
    auto trimmed = text.trim();

    if (trimmed.startsWithIgnoreCase ("v"))
        trimmed = trimmed.substring (1);

    const auto tokens = juce::StringArray::fromTokens (trimmed, ".", {});

    std::array<int, 3> version { 0, 0, 0 };
    for (int i = 0; i < 3 && i < tokens.size(); ++i)
        version[static_cast<size_t> (i)] = tokens[i].getIntValue();

    return version;
}

/// True when @p remote describes a strictly newer version than @p current.
inline bool isNewerVersion (const juce::String& remote, const juce::String& current)
{
    return parseVersion (remote) > parseVersion (current);
}

} // namespace audium
