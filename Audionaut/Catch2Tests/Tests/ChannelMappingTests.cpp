#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Resource/ChannelMapping.h"

using namespace audium;

SCENARIO("channel mappping scenario", "[engine][channel][mapping]")
{
    GIVEN("channel mapping")
    {
        auto mapping = std::make_unique<audium::ChannelMapping>();

        WHEN("mapping source 7 to destination 3")
        {
            mapping->setOutputChannelMapping(7, 3);

            THEN("source 7 is mapped to destination 3")
            {
                REQUIRE(mapping->getSourceChannel() == 7);
                REQUIRE(mapping->getDestinationChannel() == 3);
            }
        }
    }
}

SCENARIO("channel mapping json round trip", "[engine][channel][mapping][json]")
{
    GIVEN("a mapping from source 7 to destination 3")
    {
        ChannelMapping mapping;
        mapping.setOutputChannelMapping(7, 3);

        WHEN("it is written to json and read back into a fresh mapping")
        {
            json output;
            REQUIRE(mapping.writeToJson(output));

            ChannelMapping copy;
            auto ok = copy.readFromJson(output, true);

            THEN("reading succeeds and yields an equal mapping")
            {
                REQUIRE(ok);
                REQUIRE(copy.getSourceChannel() == 7);
                REQUIRE(copy.getDestinationChannel() == 3);
            }
        }

        WHEN("a legacy channel_mapping array is read")
        {
            json legacy = { { "channel_mapping", json::array({ 3 }) } };
            ChannelMapping copy;
            auto ok = copy.readFromJson(legacy, true);

            THEN("reading succeeds and maps source 0 to the listed destination")
            {
                REQUIRE(ok);
                REQUIRE(copy.getSourceChannel() == 0);
                REQUIRE(copy.getDestinationChannel() == 3);
            }
        }

        WHEN("json without any mapping is read")
        {
            json empty = json::object();
            ChannelMapping copy;

            THEN("reading fails")
            {
                REQUIRE_FALSE(copy.readFromJson(empty, true));
            }
        }

        WHEN("json with a mapping of the wrong type is read")
        {
            json malformed = { { "source_channel", "left" }, { "destination_channel", 3 } };
            ChannelMapping copy;

            THEN("reading fails instead of throwing")
            {
                REQUIRE_FALSE(copy.readFromJson(malformed, true));
            }
        }

        WHEN("a legacy channel_mapping that is not an array is read")
        {
            json malformed = { { "channel_mapping", 3 } };
            ChannelMapping copy;

            THEN("reading fails instead of throwing")
            {
                REQUIRE_FALSE(copy.readFromJson(malformed, true));
            }
        }
    }
}
