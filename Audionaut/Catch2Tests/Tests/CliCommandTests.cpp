#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <nlohmann/json.hpp>

#include "Cli/Commands/Commands.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/AudiumEngine.h"

using namespace audium;

// The run* functions own the whole headless lifecycle (MessageManager,
// engine, teardown) per invocation, so these tests must not hold their own
// MessageManager or engine across calls - they drive the commands exactly
// like main() does and inspect the results on disk.

namespace {

juce::ArgumentList makeArgs (const juce::String& commandLine)
{
    return juce::ArgumentList ("audionaut-cli", commandLine);
}

juce::File makeWorkDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("audionaut-cli-tests");
    dir.deleteRecursively();
    REQUIRE (dir.createDirectory());
    return dir;
}

const auto testFilesDir = juce::String (CURRENT_SOURCE_DIR) + "/TestFiles/";

// Parses a saved Project.json (tolerating the writer's trailing NUL, which
// strict parsers reject).
nlohmann::json readProjectJson (const juce::File& project)
{
    auto text = project.getChildFile ("Project.json").loadFileAsString().toStdString();
    while (! text.empty() && (text.back() == '\0' || text.back() == '\n'))
        text.pop_back();
    return nlohmann::json::parse (text);
}

// Empty containers are omitted from the persisted JSON, hence the .value()
// fallbacks for tracks without clips or resource groups.
int countPlayListItems (const nlohmann::json& projectJson)
{
    int count = 0;
    for (auto& track : projectJson["audium"]["audio_tracks"])
        count += (int) track.value ("play_list_vector", nlohmann::json::array()).size();
    return count;
}

std::vector<std::string> allRegionNames (const nlohmann::json& projectJson)
{
    std::vector<std::string> names;
    for (auto& track : projectJson["audium"]["audio_tracks"])
        for (auto& group : track.value ("resource_groups", nlohmann::json::array()))
            for (auto& region : group.value ("regions", nlohmann::json::array()))
                names.push_back (region["name"].get<std::string>());
    return names;
}

std::vector<double> clipPositionsClocks (const nlohmann::json& projectJson)
{
    std::vector<double> positions;
    for (auto& track : projectJson["audium"]["audio_tracks"])
        for (auto& item : track.value ("play_list_vector", nlohmann::json::array()))
            positions.push_back (item["position_clocks"].get<double>());
    return positions;
}

// start/end (source-relative seconds) of the region with the given name
std::pair<double, double> regionRangeSeconds (const nlohmann::json& projectJson,
                                              const std::string& name)
{
    for (auto& track : projectJson["audium"]["audio_tracks"])
        for (auto& group : track.value ("resource_groups", nlohmann::json::array()))
            for (auto& region : group.value ("regions", nlohmann::json::array()))
                if (region["name"] == name)
                    return { region["start"].get<double>(), region["end"].get<double>() };
    return { -1.0, -1.0 };
}

} // namespace

SCENARIO ("cli option parsing", "[cli]")
{
    GIVEN ("a command line with = and space separated option values") {
        auto args = makeArgs ("create /tmp/foo.audium --channels 4 --mode=random rest");

        WHEN ("the options are taken") {
            auto channels = cli::takeOptionValue (args, "--channels");
            auto mode = cli::takeOptionValue (args, "--mode");

            THEN ("both forms yield their value and only plain args remain") {
                REQUIRE (channels == "4");
                REQUIRE (mode == "random");

                auto plain = cli::getPlainArguments (args);
                REQUIRE (plain.size() == 2);
                REQUIRE (plain[0] == "/tmp/foo.audium");
                REQUIRE (plain[1] == "rest");
            }
        }
    }
}

SCENARIO ("cli create and info", "[cli]")
{
    auto workDir = makeWorkDirectory();
    auto project = workDir.getChildFile ("created.audium");

    cli::CliContext context;
    context.quiet = true;

    GIVEN ("a create invocation") {
        auto exitCode = cli::runCreate (makeArgs ("create " + project.getFullPathName() + " --channels 2"), context);

        THEN ("a valid project package exists") {
            REQUIRE (exitCode == cli::exitOk);
            REQUIRE (ProjectFileStore::isValidProjectStructure (project));
        }

        WHEN ("info runs on it") {
            REQUIRE (cli::runInfo (makeArgs ("info " + project.getFullPathName()), context) == cli::exitOk);
        }

        WHEN ("create runs again on the same path") {
            THEN ("it refuses to overwrite") {
                REQUIRE (cli::runCreate (makeArgs ("create " + project.getFullPathName()), context)
                         == cli::exitFailure);
            }
        }
    }

    GIVEN ("a missing project") {
        THEN ("info fails with a usage error") {
            REQUIRE (cli::runInfo (makeArgs ("info " + workDir.getChildFile ("nope.audium").getFullPathName()),
                                   context)
                     == cli::exitUsage);
        }
    }

    workDir.deleteRecursively();
}

SCENARIO ("cli split and create-region", "[cli][region]")
{
    auto workDir = makeWorkDirectory();
    auto project = workDir.getChildFile ("regions.audium");
    auto audioFile = juce::File (testFilesDir + "sine-0dB.wav");
    REQUIRE (audioFile.existsAsFile());

    cli::CliContext context;
    context.quiet = true;

    GIVEN ("a project with a one-second clip at the timeline start") {
        REQUIRE (cli::runCreate (makeArgs ("create " + project.getFullPathName() + " --channels 1"), context)
                 == cli::exitOk);
        REQUIRE (cli::runImport (makeArgs ("import " + project.getFullPathName() + " "
                                           + audioFile.getFullPathName()),
                                 context)
                 == cli::exitOk);
        auto baselineItems = countPlayListItems (readProjectJson (project));

        WHEN ("split runs at 0.5 seconds") {
            REQUIRE (cli::runSplit (makeArgs ("split " + project.getFullPathName()
                                              + " --at 0.5 --unit seconds"),
                                    context)
                     == cli::exitOk);

            THEN ("the clip became two and both pieces have derived region names") {
                auto json = readProjectJson (project);
                REQUIRE (countPlayListItems (json) == baselineItems + 1);

                auto names = allRegionNames (json);
                REQUIRE (std::count (names.begin(), names.end(), "sine-0dB-01") == 1);
                REQUIRE (std::count (names.begin(), names.end(), "sine-0dB-02") == 1);
            }
        }

        WHEN ("split runs at beat 2 (1-based, half a second at 120 BPM)") {
            REQUIRE (cli::runSplit (makeArgs ("split " + project.getFullPathName()
                                              + " --at 2 --unit beats"),
                                    context)
                     == cli::exitOk);

            THEN ("the clip became two") {
                REQUIRE (countPlayListItems (readProjectJson (project)) == baselineItems + 1);
            }
        }

        WHEN ("split targets a position outside every clip") {
            THEN ("it fails without touching the project") {
                REQUIRE (cli::runSplit (makeArgs ("split " + project.getFullPathName()
                                                  + " --at 10 --unit seconds"),
                                        context)
                         == cli::exitFailure);
                REQUIRE (countPlayListItems (readProjectJson (project)) == baselineItems);
            }
        }

        WHEN ("split is missing --at") {
            REQUIRE (cli::runSplit (makeArgs ("split " + project.getFullPathName()), context)
                     == cli::exitUsage);
        }

        WHEN ("create-region covers the middle of the clip") {
            REQUIRE (cli::runCreateRegion (makeArgs ("create-region " + project.getFullPathName()
                                                     + " --name chorus --start 0.25 --end 0.75 --unit seconds"),
                                           context)
                     == cli::exitOk);

            THEN ("a region with that name exists and the arrangement is unchanged") {
                auto json = readProjectJson (project);
                auto names = allRegionNames (json);
                REQUIRE (std::count (names.begin(), names.end(), "chorus") == 1);
                REQUIRE (countPlayListItems (json) == baselineItems);
            }
        }

        WHEN ("create-region's range reaches beyond the clip") {
            REQUIRE (cli::runCreateRegion (makeArgs ("create-region " + project.getFullPathName()
                                                     + " --name tail --start 0.5 --end 2.0 --unit seconds"),
                                           context)
                     == cli::exitFailure);
        }

        WHEN ("create-region is missing its name or range") {
            REQUIRE (cli::runCreateRegion (makeArgs ("create-region " + project.getFullPathName()
                                                     + " --start 1 --end 2"),
                                           context)
                     == cli::exitUsage);
            REQUIRE (cli::runCreateRegion (makeArgs ("create-region " + project.getFullPathName()
                                                     + " --name x --start 2 --end 1"),
                                           context)
                     == cli::exitUsage);
        }
    }

    workDir.deleteRecursively();
}

SCENARIO ("cli clip editing and set-region", "[cli][region]")
{
    auto workDir = makeWorkDirectory();
    auto project = workDir.getChildFile ("clips.audium");
    auto audioFile = juce::File (testFilesDir + "sine-0dB.wav");
    REQUIRE (audioFile.existsAsFile());

    cli::CliContext context;
    context.quiet = true;

    auto projectArg = project.getFullPathName();

    GIVEN ("a project with a one-second clip (region \"sine-0dB\") at the start") {
        REQUIRE (cli::runCreate (makeArgs ("create " + projectArg + " --channels 1"), context)
                 == cli::exitOk);
        REQUIRE (cli::runImport (makeArgs ("import " + projectArg + " " + audioFile.getFullPathName()),
                                 context)
                 == cli::exitOk);

        WHEN ("the clip is moved to 2 seconds (96 clocks at 120 BPM)") {
            REQUIRE (cli::runMoveClip (makeArgs ("move-clip " + projectArg
                                                 + " --region sine-0dB --to 2 --unit seconds"),
                                       context)
                     == cli::exitOk);

            THEN ("its persisted position moved") {
                auto positions = clipPositionsClocks (readProjectJson (project));
                REQUIRE (positions.size() == 1);
                REQUIRE (positions[0] == 96.0);
            }
        }

        WHEN ("the region is placed a second time at 5 seconds") {
            REQUIRE (cli::runPlaceClip (makeArgs ("place-clip " + projectArg
                                                  + " --region sine-0dB --at 5 --unit seconds"),
                                        context)
                     == cli::exitOk);

            THEN ("two clips of the same region exist") {
                REQUIRE (clipPositionsClocks (readProjectJson (project)).size() == 2);
            }

            AND_WHEN ("all placements are removed by region name") {
                REQUIRE (cli::runRemoveClip (makeArgs ("remove-clip " + projectArg
                                                       + " --region sine-0dB"),
                                             context)
                         == cli::exitOk);

                THEN ("the timeline is empty but the region survives") {
                    auto json = readProjectJson (project);
                    REQUIRE (clipPositionsClocks (json).empty());
                    auto names = allRegionNames (json);
                    REQUIRE (std::count (names.begin(), names.end(), "sine-0dB") == 1);
                }

                AND_WHEN ("unused regions are cleaned up") {
                    REQUIRE (cli::runCleanupRegions (makeArgs ("cleanup-regions " + projectArg),
                                                     context)
                             == cli::exitOk);

                    THEN ("the orphaned region is gone") {
                        REQUIRE (allRegionNames (readProjectJson (project)).empty());
                    }
                }
            }

            AND_WHEN ("--delete-region is asked for while another clip still uses the region") {
                REQUIRE (cli::runRemoveClip (makeArgs ("remove-clip " + projectArg
                                                       + " --at 5 --unit seconds --delete-region"),
                                             context)
                         == cli::exitFailure);
            }
        }

        WHEN ("a clip is removed by position") {
            REQUIRE (cli::runRemoveClip (makeArgs ("remove-clip " + projectArg
                                                   + " --at 0.5 --unit seconds"),
                                         context)
                     == cli::exitOk);

            THEN ("the timeline is empty") {
                REQUIRE (clipPositionsClocks (readProjectJson (project)).empty());
            }
        }

        WHEN ("nothing matches the clip address") {
            REQUIRE (cli::runRemoveClip (makeArgs ("remove-clip " + projectArg
                                                   + " --at 30 --unit seconds"),
                                         context)
                     == cli::exitFailure);
            REQUIRE (cli::runMoveClip (makeArgs ("move-clip " + projectArg
                                                 + " --region nope --to 1"),
                                       context)
                     == cli::exitFailure);
        }

        WHEN ("the region is retrimmed to its first half and renamed") {
            REQUIRE (cli::runSetRegion (makeArgs ("set-region " + projectArg
                                                  + " --region sine-0dB --length 0.5 --unit seconds"
                                                  + " --rename lead"),
                                        context)
                     == cli::exitOk);

            THEN ("the persisted range and name changed") {
                auto json = readProjectJson (project);
                auto range = regionRangeSeconds (json, "lead");
                REQUIRE (range.first == 0.0);
                REQUIRE (range.second == 0.5);
            }
        }

        WHEN ("a retrim reaches past the source audio") {
            REQUIRE (cli::runSetRegion (makeArgs ("set-region " + projectArg
                                                  + " --region sine-0dB --end 30 --unit seconds"),
                                        context)
                     == cli::exitOk);

            THEN ("the range is clamped to the one-second source") {
                auto range = regionRangeSeconds (readProjectJson (project), "sine-0dB");
                REQUIRE (range.second == 1.0);
            }
        }

        WHEN ("clip gain is set in dB on one channel and linear on all") {
            REQUIRE (cli::runClipGain (makeArgs ("clip-gain " + projectArg
                                                 + " --region sine-0dB --gain -6 --db --channel 0"),
                                       context)
                     == cli::exitOk);

            THEN ("the persisted gain vector holds -6 dB") {
                auto json = readProjectJson (project);
                bool found = false;
                for (auto& track : json["audium"]["audio_tracks"])
                    for (auto& item : track.value ("play_list_vector", nlohmann::json::array()))
                        if (item.contains ("gain_vector")) {
                            REQUIRE (item["gain_vector"].size() == 1);
                            REQUIRE (item["gain_vector"][0].get<double>()
                                     == Catch::Approx (0.5011872336272722));
                            found = true;
                        }
                REQUIRE (found);
            }
        }

        WHEN ("clip fades are set in seconds with a linear fade-in curve") {
            REQUIRE (cli::runClipFades (makeArgs ("clip-fades " + projectArg
                                                  + " --region sine-0dB --unit seconds"
                                                  + " --fade-in 0.25 --fade-out 0.1 --fade-in-curve 1"),
                                        context)
                     == cli::exitOk);

            THEN ("the persisted fades hold the clock equivalents") {
                auto json = readProjectJson (project);
                bool found = false;
                for (auto& track : json["audium"]["audio_tracks"])
                    for (auto& item : track.value ("play_list_vector", nlohmann::json::array()))
                        if (item.contains ("fade_in_clocks")) {
                            // 0.25 s / 0.1 s at 120 BPM = 12 / 4.8 clocks
                            REQUIRE (item["fade_in_clocks"].get<double>() == Catch::Approx (12.0));
                            REQUIRE (item["fade_out_clocks"].get<double>() == Catch::Approx (4.8));
                            REQUIRE (item["fade_in_curve"].get<double>() == Catch::Approx (1.0));
                            REQUIRE (! item.contains ("fade_out_curve")); // still the default
                            found = true;
                        }
                REQUIRE (found);
            }
        }

        WHEN ("clip-gain and clip-fades get unusable options") {
            REQUIRE (cli::runClipGain (makeArgs ("clip-gain " + projectArg + " --region sine-0dB"),
                                       context)
                     == cli::exitUsage);
            REQUIRE (cli::runClipGain (makeArgs ("clip-gain " + projectArg
                                                 + " --region sine-0dB --gain 1 --channel 7"),
                                       context)
                     == cli::exitUsage);
            REQUIRE (cli::runClipFades (makeArgs ("clip-fades " + projectArg + " --region sine-0dB"),
                                        context)
                     == cli::exitUsage);
            REQUIRE (cli::runClipFades (makeArgs ("clip-fades " + projectArg
                                                  + " --region sine-0dB --fade-in -1"),
                                        context)
                     == cli::exitUsage);
        }

        WHEN ("set-region gets contradictory or missing options") {
            REQUIRE (cli::runSetRegion (makeArgs ("set-region " + projectArg
                                                  + " --region sine-0dB --end 2 --length 1"),
                                        context)
                     == cli::exitUsage);
            REQUIRE (cli::runSetRegion (makeArgs ("set-region " + projectArg + " --region sine-0dB"),
                                        context)
                     == cli::exitUsage);
            REQUIRE (cli::runSetRegion (makeArgs ("set-region " + projectArg
                                                  + " --region nope --length 1"),
                                        context)
                     == cli::exitFailure);
        }
    }

    workDir.deleteRecursively();
}

SCENARIO ("cli import and export round trip", "[cli]")
{
    auto workDir = makeWorkDirectory();
    auto project = workDir.getChildFile ("roundtrip.audium");
    auto outputFile = workDir.getChildFile ("bounce.wav");
    auto audioFile = juce::File (testFilesDir + "sine-0dB.wav");
    REQUIRE (audioFile.existsAsFile());

    cli::CliContext context;
    context.quiet = true;

    GIVEN ("a project with an imported sine file") {
        REQUIRE (cli::runCreate (makeArgs ("create " + project.getFullPathName() + " --channels 1"), context)
                 == cli::exitOk);
        REQUIRE (cli::runImport (makeArgs ("import " + project.getFullPathName() + " "
                                           + audioFile.getFullPathName()),
                                 context)
                 == cli::exitOk);

        WHEN ("one region is exported while its clip carries a fade-in") {
            auto clipFile = workDir.getChildFile ("clip.wav");
            REQUIRE (cli::runClipFades (makeArgs ("clip-fades " + project.getFullPathName()
                                                  + " --region sine-0dB --fade-in 0.4 --unit seconds"),
                                        context)
                     == cli::exitOk);
            REQUIRE (cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                               + clipFile.getFullPathName() + " --region sine-0dB"
                                               + " --channels 1"),
                                     context)
                     == cli::exitOk);

            THEN ("the bounce is the region's length and dry - the clip's fade is not rendered") {
                REQUIRE (clipFile.existsAsFile());

                juce::AudioFormatManager formatManager;
                formatManager.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (clipFile));
                REQUIRE (reader != nullptr);

                // the one-second region, within a block of tolerance
                REQUIRE (reader->lengthInSamples
                         == Catch::Approx (reader->sampleRate).margin (reader->sampleRate * 0.05));

                juce::AudioBuffer<float> buffer (1, (int) reader->lengthInSamples);
                reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, false);

                auto samples = buffer.getNumSamples();
                auto headMagnitude = buffer.getMagnitude (0, 0, samples / 10);
                auto tailMagnitude = buffer.getMagnitude (0, samples / 2, samples / 2);

                REQUIRE (tailMagnitude > 0.9f); // the 0 dB sine
                REQUIRE (headMagnitude > 0.9f); // full scale from the first samples: no fade
            }
        }

        WHEN ("an unknown region is exported") {
            REQUIRE (cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                               + outputFile.getFullPathName() + " --region nope"),
                                     context)
                     == cli::exitFailure);
        }

        WHEN ("the project is exported") {
            REQUIRE (cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                               + outputFile.getFullPathName() + " --channels 1"),
                                     context)
                     == cli::exitOk);

            THEN ("the bounce contains the (non-silent) sine") {
                REQUIRE (outputFile.existsAsFile());

                juce::AudioFormatManager formatManager;
                formatManager.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (outputFile));
                REQUIRE (reader != nullptr);
                REQUIRE (reader->lengthInSamples > 0);

                juce::AudioBuffer<float> buffer (1, (int) reader->lengthInSamples);
                reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, false);
                REQUIRE (buffer.getMagnitude (0, 0, buffer.getNumSamples()) > 0.5f);
            }
        }
    }

    workDir.deleteRecursively();
}

SCENARIO ("cli separate adds four stem tracks with the fake backend", "[cli][separation]")
{
    auto workDir = makeWorkDirectory();
    auto project = workDir.getChildFile ("separate.audium");
    auto audioFile = juce::File (testFilesDir + "sine-0dB.wav");
    REQUIRE (audioFile.existsAsFile());

    cli::CliContext context;
    context.quiet = true;

    GIVEN ("a project with an imported sine file on track 1") {
        REQUIRE (cli::runCreate (makeArgs ("create " + project.getFullPathName() + " --channels 1"), context)
                 == cli::exitOk);
        REQUIRE (cli::runImport (makeArgs ("import " + project.getFullPathName() + " "
                                           + audioFile.getFullPathName() + " --position 2"),
                                 context)
                 == cli::exitOk);

        const auto before = readProjectJson (project);
        const auto tracksBefore = before["audium"]["audio_tracks"].size();
        const auto clipsBefore = countPlayListItems (before);

        WHEN ("the clip is separated with the fake backend") {
            REQUIRE (cli::runSeparate (makeArgs ("separate " + project.getFullPathName()
                                                 + " --track 1 --backend fake --threads 2"),
                                       context)
                     == cli::exitOk);

            THEN ("the saved project has four more tracks, one clip each, named after the stems") {
                const auto after = readProjectJson (project);
                const auto tracks = after["audium"]["audio_tracks"];
                REQUIRE (tracks.size() == tracksBefore + 4);
                REQUIRE (countPlayListItems (after) == clipsBefore + 4);

                const auto lastName = tracks.back().value ("name", std::string());
                REQUIRE (lastName.find ("Vocals") != std::string::npos);
            }

            THEN ("the stem files live in the package's audio folder") {
                auto audioDir = project.getChildFile ("Media").getChildFile ("Audio");
                REQUIRE (audioDir.getChildFile ("sine-0dB - Vocals.wav").existsAsFile());
                REQUIRE (audioDir.getChildFile ("sine-0dB - Drums.wav").existsAsFile());
            }
        }

        WHEN ("an unknown backend is asked for") {
            REQUIRE (cli::runSeparate (makeArgs ("separate " + project.getFullPathName() + " --backend nope"), context)
                     == cli::exitUsage);
        }

        WHEN ("the model file is missing") {
            const auto result = cli::runSeparate (makeArgs ("separate " + project.getFullPathName()
                                                            + " --model " + workDir.getChildFile ("nope.bin").getFullPathName()),
                                                  context);

            THEN ("it fails before touching the project") {
                // model_missing when Demucs is compiled in, demucs_unavailable otherwise
                REQUIRE ((result == cli::exitFailure || result == cli::exitUnavailable));
                REQUIRE (readProjectJson (project)["audium"]["audio_tracks"].size() == tracksBefore);
            }
        }
    }

    workDir.deleteRecursively();
}
