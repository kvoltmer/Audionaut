//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>

#include "Util/VersionCompare.h"

using namespace audium;

SCENARIO("version strings parse and compare across channels", "[util][version]")
{
    THEN("GitHub tags and App Store versions parse alike")
    {
        REQUIRE(parseVersion("v1.2.3") == std::array<int, 3>{1, 2, 3});
        REQUIRE(parseVersion("1.2.3") == std::array<int, 3>{1, 2, 3});
        REQUIRE(parseVersion(" v1.2.3 ") == std::array<int, 3>{1, 2, 3});
    }

    THEN("missing components read as zero")
    {
        REQUIRE(parseVersion("1.4") == std::array<int, 3>{1, 4, 0});
        REQUIRE(parseVersion("2") == std::array<int, 3>{2, 0, 0});
        REQUIRE(parseVersion("") == std::array<int, 3>{0, 0, 0});
    }

    THEN("ordering is numeric per component, not lexical")
    {
        REQUIRE(isNewerVersion("1.4.1", "1.4.0"));
        REQUIRE(isNewerVersion("1.10.0", "1.9.9"));
        REQUIRE(isNewerVersion("2.0.0", "1.99.99"));
        REQUIRE(isNewerVersion("v1.4.1", "1.4.0"));

        REQUIRE_FALSE(isNewerVersion("1.4.0", "1.4.0"));
        REQUIRE_FALSE(isNewerVersion("1.2.2", "1.4.0"));
        REQUIRE_FALSE(isNewerVersion("v1.2.2", "1.4.0"));
    }
}
