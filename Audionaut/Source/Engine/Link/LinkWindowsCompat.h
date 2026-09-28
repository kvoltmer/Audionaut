//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <cstdint>

#if JUCE_WINDOWS
namespace audium::detail
{
inline constexpr std::uint64_t linkWindowsNetworkByteOrder64(const std::uint64_t value) noexcept
{
#if JUCE_LITTLE_ENDIAN
    return ((value & 0x00000000000000FFull) << 56)
         | ((value & 0x000000000000FF00ull) << 40)
         | ((value & 0x0000000000FF0000ull) << 24)
         | ((value & 0x00000000FF000000ull) << 8)
         | ((value & 0x000000FF00000000ull) >> 8)
         | ((value & 0x0000FF0000000000ull) >> 24)
         | ((value & 0x00FF000000000000ull) >> 40)
         | ((value & 0xFF00000000000000ull) >> 56);
#else
    return value;
#endif
}
}

#ifndef htonll
 #define htonll(x) audium::detail::linkWindowsNetworkByteOrder64(static_cast<std::uint64_t>(x))
#endif

#ifndef ntohll
 #define ntohll(x) audium::detail::linkWindowsNetworkByteOrder64(static_cast<std::uint64_t>(x))
#endif
#endif
