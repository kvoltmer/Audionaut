#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <iostream>
#include <sstream>
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

        // track ids are indices, so a track created now gets the old count as id
        const auto baselineTracks = readProjectJson (project)["audium"]["audio_tracks"].size();
        auto clipsOnTrack = [] (const nlohmann::json& track) {
            return track.value ("play_list_vector", nlohmann::json::array());
        };

        WHEN ("the clip is moved to a new track without --to") {
            REQUIRE (cli::runMoveClip (makeArgs ("move-clip " + projectArg
                                                 + " --region sine-0dB --to-track new"),
                                       context)
                     == cli::exitOk);

            THEN ("the new track holds the only clip, at the same position") {
                auto json = readProjectJson (project);
                auto tracks = json["audium"]["audio_tracks"];
                REQUIRE (tracks.size() == baselineTracks + 1);
                REQUIRE (countPlayListItems (json) == 1);
                auto moved = clipsOnTrack (tracks.back());
                REQUIRE (moved.size() == 1);
                REQUIRE (moved[0]["position_clocks"].get<double>() == 0.0);
            }

            THEN ("the new track got a track colour, not the pink default") {
                auto tracks = readProjectJson (project)["audium"]["audio_tracks"];
                REQUIRE (tracks.back()["colour"].get<std::string>()
                         != juce::Colours::pink.toString().toStdString());
            }
        }

        WHEN ("two placements are moved to the same other track one by one") {
            REQUIRE (cli::runPlaceClip (makeArgs ("place-clip " + projectArg
                                                  + " --region sine-0dB --at 5 --unit seconds"),
                                        context)
                     == cli::exitOk);
            REQUIRE (cli::runMoveClip (makeArgs ("move-clip " + projectArg
                                                 + " --at 5 --unit seconds --to-track new"),
                                       context)
                     == cli::exitOk);
            REQUIRE (cli::runMoveClip (makeArgs ("move-clip " + projectArg
                                                 + " --at 0.5 --unit seconds --to 2 --to-track "
                                                 + juce::String ((int) baselineTracks)),
                                       context)
                     == cli::exitOk);

            THEN ("both land on the new track and share one resource group there") {
                auto json = readProjectJson (project);
                auto tracks = json["audium"]["audio_tracks"];
                REQUIRE (tracks.size() == baselineTracks + 1);
                REQUIRE (countPlayListItems (json) == 2);
                REQUIRE (clipsOnTrack (tracks.back()).size() == 2);
                auto groups = tracks.back().value ("resource_groups", nlohmann::json::array());
                auto groupsWithResources = std::count_if (groups.begin(), groups.end(), [] (auto& group) {
                    return ! group.value ("resources", nlohmann::json::array()).empty();
                });
                REQUIRE (groupsWithResources == 1);
            }
        }

        WHEN ("a clip is moved to a track that does not exist") {
            REQUIRE (cli::runMoveClip (makeArgs ("move-clip " + projectArg
                                                 + " --region sine-0dB --to-track 7"),
                                       context)
                     == cli::exitFailure);
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

        WHEN ("the project is exported at a bit depth the WAV writer rejects") {
            nlohmann::json envelope;
            context.envelopeSink = [&envelope] (const nlohmann::json& produced) { envelope = produced; };
            auto exitCode = cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                                      + outputFile.getFullPathName()
                                                      + " --channels 1 --bit-depth 12"),
                                            context);
            context.envelopeSink = nullptr;

            THEN ("the verb fails, says why, and leaves no file behind") {
                REQUIRE (exitCode != cli::exitOk);
                REQUIRE (envelope["ok"] == false);
                auto message = envelope["error"]["message"].get<std::string>();
                REQUIRE (message.find ("12") != std::string::npos);
                REQUIRE_FALSE (outputFile.existsAsFile());
            }
        }

        WHEN ("a failing export targets a file left over from an earlier run") {
            REQUIRE (outputFile.replaceWithText ("stale"));
            nlohmann::json envelope;
            context.envelopeSink = [&envelope] (const nlohmann::json& produced) { envelope = produced; };
            auto exitCode = cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                                      + outputFile.getFullPathName()
                                                      + " --channels 1 --bit-depth 12"),
                                            context);
            context.envelopeSink = nullptr;

            THEN ("the leftover is not passed off as the result") {
                REQUIRE (exitCode != cli::exitOk);
                REQUIRE (envelope["ok"] == false);
                REQUIRE (outputFile.loadFileAsString() == "stale");
            }
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

SCENARIO ("cli export picks the format from the output extension", "[cli][export][flac]")
{
    auto workDir = makeWorkDirectory();
    auto project = workDir.getChildFile ("formats.audium");

    cli::CliContext context;
    context.quiet = true;

    nlohmann::json envelope;
    context.envelopeSink = [&envelope] (const nlohmann::json& produced) { envelope = produced; };

    auto readerFor = [] (const juce::File& file) {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        return std::unique_ptr<juce::AudioFormatReader> (formatManager.createReaderFor (file));
    };

    GIVEN ("a project with a 32-bit float file imported") {
        // one second of a quiet constant, written as 32-bit float
        auto source = workDir.getChildFile ("float-source.wav");
        {
            std::unique_ptr<juce::OutputStream> stream (source.createOutputStream());
            REQUIRE (stream != nullptr);
            juce::WavAudioFormat wav;
            auto writer = wav.createWriterFor (stream, juce::AudioFormatWriter::Options{}
                                                           .withSampleRate (44100.0)
                                                           .withNumChannels (1)
                                                           .withBitsPerSample (32)
                                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
            REQUIRE (writer != nullptr);
            juce::AudioBuffer<float> buffer (1, 44100);
            for (auto s = 0; s < buffer.getNumSamples(); s++)
                buffer.setSample (0, s, 0.25f);
            REQUIRE (writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()));
        }
        REQUIRE (cli::runCreate (makeArgs ("create " + project.getFullPathName() + " --channels 1"), context)
                 == cli::exitOk);
        REQUIRE (cli::runImport (makeArgs ("import " + project.getFullPathName() + " " + source.getFullPathName()),
                                 context)
                 == cli::exitOk);
        auto regionName = source.getFileNameWithoutExtension();

        WHEN ("the project is exported to a .flac file") {
            auto output = workDir.getChildFile ("mix.flac");
            REQUIRE (cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                               + output.getFullPathName() + " --channels 1"),
                                     context)
                     == cli::exitOk);

            THEN ("a FLAC file is written and the result says so") {
                REQUIRE (envelope["result"]["format"] == "flac");
                auto reader = readerFor (output);
                REQUIRE (reader != nullptr);
                REQUIRE (reader->getFormatName() == "FLAC file");
                REQUIRE (reader->lengthInSamples > 0);
            }
        }

        WHEN ("the float region is exported to FLAC without a bit depth") {
            auto output = workDir.getChildFile ("region.flac");
            REQUIRE (cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                               + output.getFullPathName() + " --region " + regionName),
                                     context)
                     == cli::exitOk);

            THEN ("it is written as deep as FLAC goes: 24 bits") {
                REQUIRE (envelope["result"]["bitDepth"] == 24);
                auto reader = readerFor (output);
                REQUIRE (reader != nullptr);
                REQUIRE (reader->bitsPerSample == 24);
            }
        }

        WHEN ("a 32-bit FLAC is asked for explicitly") {
            auto output = workDir.getChildFile ("deep.flac");
            auto exitCode = cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                                      + output.getFullPathName() + " --channels 1 --bit-depth 32"),
                                            context);

            THEN ("the verb refuses instead of quietly writing 24 bits") {
                REQUIRE (exitCode == cli::exitUsage);
                REQUIRE_FALSE (output.existsAsFile());
            }
        }

        WHEN ("the project is exported multi-mono to FLAC") {
            auto output = workDir.getChildFile ("stems.flac");
            REQUIRE (cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                               + output.getFullPathName() + " --multi-mono"),
                                     context)
                     == cli::exitOk);

            THEN ("the result names the first FLAC mono file") {
                auto first = workDir.getChildFile ("stems-01.flac");
                REQUIRE (envelope["result"]["outputFile"] == first.getFullPathName().toStdString());
                REQUIRE (first.existsAsFile());
                REQUIRE_FALSE (output.existsAsFile());
            }
        }

        WHEN ("the project is exported to .aiff and to .ogg") {
            auto aiff = workDir.getChildFile ("mix.aiff");
            REQUIRE (cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                               + aiff.getFullPathName() + " --channels 1 --bit-depth 16"),
                                     context)
                     == cli::exitOk);
            auto aiffResult = envelope["result"];

            auto ogg = workDir.getChildFile ("mix.ogg");
            REQUIRE (cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                               + ogg.getFullPathName() + " --channels 1"),
                                     context)
                     == cli::exitOk);
            auto oggResult = envelope["result"];

            THEN ("each is written in its format; the Ogg reports its bit rate instead of a bit depth") {
                REQUIRE (aiffResult["format"] == "aiff");
                REQUIRE (aiffResult["bitDepth"] == 16);
                REQUIRE (readerFor (aiff)->getFormatName() == "AIFF file");

                REQUIRE (oggResult["format"] == "ogg");
                REQUIRE (oggResult["bitrateKbps"] == 192);
                REQUIRE_FALSE (oggResult.contains ("bitDepth"));
                REQUIRE (readerFor (ogg)->getFormatName() == "Ogg-Vorbis file");
            }
        }

        WHEN ("an Ogg export is given a bit rate") {
            auto ogg = workDir.getChildFile ("small.ogg");
            REQUIRE (cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                               + ogg.getFullPathName() + " --channels 1 --bitrate 96"),
                                     context)
                     == cli::exitOk);

            THEN ("the result reports it") {
                REQUIRE (envelope["result"]["bitrateKbps"] == 96);
                REQUIRE (ogg.existsAsFile());
            }
        }

        WHEN ("bit rate and bit depth are given to the wrong kind of format") {
            auto depthForOgg = cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                                         + workDir.getChildFile ("a.ogg").getFullPathName()
                                                         + " --bit-depth 24"),
                                               context);
            auto depthMessage = envelope["error"]["message"].get<std::string>();
            auto bitRateForWav = cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                                           + workDir.getChildFile ("b.wav").getFullPathName()
                                                           + " --bitrate 192"),
                                                 context);
            auto bitRateOffScale = cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                                             + workDir.getChildFile ("c.ogg").getFullPathName()
                                                             + " --bitrate 100"),
                                                   context);

            THEN ("each is a usage error and nothing is written") {
                REQUIRE (depthForOgg == cli::exitUsage);
                REQUIRE (depthMessage.find ("--bitrate") != std::string::npos);
                REQUIRE (bitRateForWav == cli::exitUsage);
                REQUIRE (bitRateOffScale == cli::exitUsage);
                for (auto name : { "a.ogg", "b.wav", "c.ogg" })
                    REQUIRE_FALSE (workDir.getChildFile (name).existsAsFile());
            }
        }

        WHEN ("the project is exported to .mp3") {
            auto mp3 = workDir.getChildFile ("mix.mp3");
            REQUIRE (cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                               + mp3.getFullPathName() + " --channels 1 --bitrate 320"),
                                     context)
                     == cli::exitOk);
            auto result = envelope["result"];

            auto offered = cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                                     + workDir.getChildFile ("d.mp3").getFullPathName()
                                                     + " --bitrate 500"),
                                           context);
            auto offeredMessage = envelope["error"]["message"].get<std::string>();
            auto wide = cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                                  + workDir.getChildFile ("e.mp3").getFullPathName()
                                                  + " --channels 3"),
                                        context);

            THEN ("an MP3 is written at the asked bit rate; other rates and channel counts are refused") {
                REQUIRE (result["format"] == "mp3");
                REQUIRE (result["bitrateKbps"] == 320);
                REQUIRE_FALSE (result.contains ("bitDepth"));
                juce::MP3AudioFormat decoder;
                std::unique_ptr<juce::AudioFormatReader> reader (decoder.createReaderFor (mp3.createInputStream().release(), true));
                REQUIRE (reader != nullptr);
                REQUIRE (reader->lengthInSamples > 0);

                // 500 kbps is an Ogg rate, not an MP3 one; the message lists MP3's
                REQUIRE (offered == cli::exitUsage);
                REQUIRE (offeredMessage.find ("320") != std::string::npos);
                REQUIRE (wide == cli::exitUsage);
                for (auto name : { "d.mp3", "e.mp3" })
                    REQUIRE_FALSE (workDir.getChildFile (name).existsAsFile());
            }
        }

        WHEN ("the output has an extension no export writes") {
            auto output = workDir.getChildFile ("mix.wma");
            auto exitCode = cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                                      + output.getFullPathName()),
                                            context);

            THEN ("it is a usage error naming the formats") {
                REQUIRE (exitCode == cli::exitUsage);
                auto message = envelope["error"]["message"].get<std::string>();
                REQUIRE (message.find (".flac") != std::string::npos);
                REQUIRE_FALSE (output.existsAsFile());
            }
        }

    }

    context.envelopeSink = nullptr;
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

            THEN ("the source track's channels are muted in the saved project") {
                const auto after = readProjectJson (project);
                const auto& source = after["audium"]["audio_tracks"][1];

                for (const auto& channel : source.value ("channels", nlohmann::json::array()))
                    REQUIRE (channel.value ("mute", false));
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

SCENARIO ("cli progress reaches a wrapper as JSON lines on stderr", "[cli][progress]")
{
    // Swap stderr for a buffer for the scope of one THEN.
    struct CapturedStderr
    {
        CapturedStderr() : saved (std::cerr.rdbuf (buffer.rdbuf())) {}
        ~CapturedStderr() { std::cerr.rdbuf (saved); }
        std::vector<std::string> lines()
        {
            std::vector<std::string> result;
            std::istringstream in (buffer.str());
            for (std::string line; std::getline (in, line);)
                result.push_back (line);
            return result;
        }
        std::ostringstream buffer;
        std::streambuf* saved;
    };

    GIVEN ("a quiet --json run that asked for --progress-json") {
        cli::CliContext context;
        context.json = true;
        context.quiet = true;
        context.progressJson = true;

        THEN ("progress is one parseable object per line, and plain logs stay quiet") {
            CapturedStderr captured;
            context.log ("chatter");
            context.progress (0.35, "Separating stems");

            const auto lines = captured.lines();
            REQUIRE (lines.size() == 1);

            const auto parsed = nlohmann::json::parse (lines[0]);
            REQUIRE (parsed["progress"].get<double>() == Catch::Approx (0.35));
            REQUIRE (parsed["message"] == "Separating stems");
        }
    }

    GIVEN ("a human run") {
        cli::CliContext context;

        THEN ("progress is the familiar percentage line") {
            CapturedStderr captured;
            context.progress (0.35, "Separating stems");
            REQUIRE (captured.lines() == std::vector<std::string> { "Separating stems 35%" });
        }
    }
}

SCENARIO ("cli progress steps", "[cli][progress]")
{
    cli::CliContext context;
    std::vector<double> fractions;
    context.progressSink = [&fractions] (double fraction, const juce::String&) { fractions.push_back (fraction); };

    cli::ProgressSteps steps (context);

    WHEN ("a callback reports finely, repeats itself and runs backwards") {
        for (auto fraction : { 0.0, 0.01, 0.049, 0.05, 0.05, 0.12, 0.08, 0.5, 1.0, 1.0 })
            steps (fraction, "Working");

        THEN ("one report per new 5% step reaches the context, starting at 0") {
            REQUIRE (fractions == std::vector<double> { 0.0, 0.05, 0.12, 0.5, 1.0 });
        }
    }
}

SCENARIO ("cli long verbs report progress from 0 to 1, once per 5% step", "[cli][progress]")
{
    auto workDir = makeWorkDirectory();
    auto project = workDir.getChildFile ("progress.audium");

    cli::CliContext context;
    context.quiet = true;

    REQUIRE (cli::runCreate (makeArgs ("create " + project.getFullPathName() + " --channels 1"), context)
             == cli::exitOk);
    REQUIRE (cli::runImport (makeArgs ("import " + project.getFullPathName() + " "
                                       + juce::File (testFilesDir + "sine-0dB.wav").getFullPathName()),
                             context)
             == cli::exitOk);

    std::vector<double> fractions;
    context.progressSink = [&fractions] (double fraction, const juce::String&) { fractions.push_back (fraction); };

    auto requireSteppedFromZeroToOne = [&fractions]
    {
        REQUIRE_FALSE (fractions.empty());
        REQUIRE (fractions.front() == Catch::Approx (0.0));
        REQUIRE (fractions.back() == Catch::Approx (1.0));

        for (size_t index = 1; index < fractions.size(); ++index)
            REQUIRE (static_cast<int> (fractions[index] * 100.0) / 5
                     > static_cast<int> (fractions[index - 1] * 100.0) / 5);
    };

    WHEN ("a clip is separated") {
        REQUIRE (cli::runSeparate (makeArgs ("separate " + project.getFullPathName()
                                             + " --track 1 --backend fake --threads 1"),
                                   context)
                 == cli::exitOk);
        requireSteppedFromZeroToOne();
    }

    WHEN ("the project is exported") {
        REQUIRE (cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                           + workDir.getChildFile ("mix.wav").getFullPathName()),
                                 context)
                 == cli::exitOk);
        requireSteppedFromZeroToOne();
    }

    WHEN ("the project is analyzed") {
        const auto exitCode = cli::runAnalyze (makeArgs ("analyze " + project.getFullPathName() + " --types sbic"),
                                               context);

        // builds without Essentia have nothing to report
        if (exitCode != cli::exitUnavailable) {
            REQUIRE (exitCode == cli::exitOk);
            requireSteppedFromZeroToOne();
        }
    }

    workDir.deleteRecursively();
}

SCENARIO ("cli remove-track and remove-channel", "[cli]")
{
    auto workDir = makeWorkDirectory();
    auto project = workDir.getChildFile ("tracks.audium");
    auto audioFile = juce::File (testFilesDir + "sine-0dB.wav");
    REQUIRE (audioFile.existsAsFile());

    cli::CliContext context;
    context.quiet = true;

    // channel count of one track in the persisted JSON (empty tracks omit the key)
    auto numChannels = [] (const nlohmann::json& projectJson, size_t trackIndex) {
        auto& track = projectJson["audium"]["audio_tracks"][trackIndex];
        return (int) track.value ("channels", nlohmann::json::array()).size();
    };
    auto numTracks = [] (const nlohmann::json& projectJson) {
        return (int) projectJson["audium"]["audio_tracks"].size();
    };

    GIVEN ("a project with a two-channel track 0 and an imported clip on track 1") {
        REQUIRE (cli::runCreate (makeArgs ("create " + project.getFullPathName() + " --channels 2"), context)
                 == cli::exitOk);
        REQUIRE (cli::runImport (makeArgs ("import " + project.getFullPathName() + " "
                                           + audioFile.getFullPathName()),
                                 context)
                 == cli::exitOk);
        REQUIRE (numTracks (readProjectJson (project)) == 2);

        WHEN ("a channel is removed from track 0") {
            REQUIRE (cli::runRemoveChannel (makeArgs ("remove-channel " + project.getFullPathName()
                                                      + " --track 0 --channel 1"),
                                            context)
                     == cli::exitOk);

            THEN ("one channel remains and the arrangement is untouched") {
                auto json = readProjectJson (project);
                REQUIRE (numChannels (json, 0) == 1);
                REQUIRE (countPlayListItems (json) == 1);
            }
        }

        WHEN ("the imported track's only channel is removed") {
            REQUIRE (cli::runRemoveChannel (makeArgs ("remove-channel " + project.getFullPathName()
                                                      + " --track 1 --channel 0"),
                                            context)
                     == cli::exitOk);

            THEN ("the empty track survives and the project still opens") {
                REQUIRE (numChannels (readProjectJson (project), 1) == 0);
                REQUIRE (cli::runInfo (makeArgs ("info " + project.getFullPathName()), context)
                         == cli::exitOk);
            }
        }

        WHEN ("the channel index is out of range") {
            REQUIRE (cli::runRemoveChannel (makeArgs ("remove-channel " + project.getFullPathName()
                                                      + " --track 0 --channel 2"),
                                            context)
                     == cli::exitUsage);
        }

        WHEN ("the track does not exist") {
            REQUIRE (cli::runRemoveChannel (makeArgs ("remove-channel " + project.getFullPathName()
                                                      + " --track 7 --channel 0"),
                                            context)
                     == cli::exitFailure);
            REQUIRE (cli::runRemoveTrack (makeArgs ("remove-track " + project.getFullPathName()
                                                    + " --track 7"),
                                          context)
                     == cli::exitFailure);
        }

        WHEN ("the imported track is removed") {
            REQUIRE (cli::runRemoveTrack (makeArgs ("remove-track " + project.getFullPathName()
                                                    + " --track 1"),
                                          context)
                     == cli::exitOk);

            THEN ("only the empty track remains and its clips are gone") {
                auto json = readProjectJson (project);
                REQUIRE (numTracks (json) == 1);
                REQUIRE (countPlayListItems (json) == 0);
                REQUIRE (cli::runInfo (makeArgs ("info " + project.getFullPathName()), context)
                         == cli::exitOk);

                AND_THEN ("the now-unreferenced audio file is still in the package") {
                    // headless opens must never trash files behind the agent's back
                    auto packaged = project.getChildFile ("Media").getChildFile ("Audio")
                                           .getChildFile (audioFile.getFileName());
                    REQUIRE (packaged.existsAsFile());
                }
            }
        }

        WHEN ("--track is missing") {
            REQUIRE (cli::runRemoveTrack (makeArgs ("remove-track " + project.getFullPathName()), context)
                     == cli::exitUsage);
            REQUIRE (cli::runRemoveChannel (makeArgs ("remove-channel " + project.getFullPathName()
                                                      + " --channel 0"),
                                            context)
                     == cli::exitUsage);
        }
    }

    workDir.deleteRecursively();
}

SCENARIO ("cli clip-speed re-pitches a clip", "[cli][clipspeed]")
{
    auto workDir = makeWorkDirectory();
    auto project = workDir.getChildFile ("speed.audium");
    auto audioFile = juce::File (testFilesDir + "sine-0dB.wav");
    REQUIRE (audioFile.existsAsFile());

    cli::CliContext context;
    context.quiet = true;

    GIVEN ("a project with an imported one-second sine") {
        REQUIRE (cli::runCreate (makeArgs ("create " + project.getFullPathName() + " --channels 1"), context)
                 == cli::exitOk);
        REQUIRE (cli::runImport (makeArgs ("import " + project.getFullPathName() + " "
                                           + audioFile.getFullPathName()),
                                 context)
                 == cli::exitOk);

        WHEN ("the clip is set to half speed by ratio") {
            REQUIRE (cli::runClipSpeed (makeArgs ("clip-speed " + project.getFullPathName()
                                                  + " --region sine-0dB --ratio 0.5"),
                                        context)
                     == cli::exitOk);

            THEN ("the saved project carries the speed ratio") {
                const auto after = readProjectJson (project);
                const auto& item = after["audium"]["audio_tracks"][1]["play_list_vector"][0];
                REQUIRE (item.value ("speed_ratio", 1.0) == Catch::Approx (0.5));
            }
        }

        WHEN ("the clip is shifted down an octave in semitones") {
            REQUIRE (cli::runClipSpeed (makeArgs ("clip-speed " + project.getFullPathName()
                                                  + " --region sine-0dB --semitones -12"),
                                        context)
                     == cli::exitOk);

            const auto after = readProjectJson (project);
            const auto& item = after["audium"]["audio_tracks"][1]["play_list_vector"][0];
            REQUIRE (item.value ("speed_ratio", 1.0) == Catch::Approx (0.5));
        }

        WHEN ("an out-of-range ratio is asked for") {
            REQUIRE (cli::runClipSpeed (makeArgs ("clip-speed " + project.getFullPathName()
                                                  + " --region sine-0dB --ratio 8"),
                                        context)
                     == cli::exitUsage);
        }

        WHEN ("no value option is given") {
            REQUIRE (cli::runClipSpeed (makeArgs ("clip-speed " + project.getFullPathName()
                                                  + " --region sine-0dB"),
                                        context)
                     == cli::exitUsage);
        }

        WHEN ("the clip is locked to the project tempo with its own tempo given") {
            REQUIRE (cli::runClipSpeed (makeArgs ("clip-speed " + project.getFullPathName()
                                                  + " --region sine-0dB --tempo 60"),
                                        context)
                     == cli::exitOk);

            THEN ("the saved clip is locked at that tempo") {
                const auto after = readProjectJson (project);
                const auto& item = after["audium"]["audio_tracks"][1]["play_list_vector"][0];
                REQUIRE (item.value ("tempo_locked", false));
                REQUIRE (item.value ("clip_tempo", 0.0) == Catch::Approx (60.0));
            }

            THEN ("a ratio is refused while locked, and accepted once released") {
                REQUIRE (cli::runClipSpeed (makeArgs ("clip-speed " + project.getFullPathName()
                                                      + " --region sine-0dB --ratio 0.5"),
                                            context)
                         == cli::exitUsage);
                REQUIRE (cli::runClipSpeed (makeArgs ("clip-speed " + project.getFullPathName()
                                                      + " --region sine-0dB --lock-tempo off --ratio 0.5"),
                                            context)
                         == cli::exitOk);

                const auto after = readProjectJson (project);
                const auto& item = after["audium"]["audio_tracks"][1]["play_list_vector"][0];
                REQUIRE_FALSE (item.value ("tempo_locked", false));
                REQUIRE (item.value ("speed_ratio", 1.0) == Catch::Approx (0.5));
            }
        }

        WHEN ("the lock is asked for without a tempo") {
            REQUIRE (cli::runClipSpeed (makeArgs ("clip-speed " + project.getFullPathName()
                                                  + " --region sine-0dB --lock-tempo on"),
                                        context)
                     == cli::exitOk);

            THEN ("the clip is locked (its tempo detected when the build can)") {
                const auto after = readProjectJson (project);
                const auto& item = after["audium"]["audio_tracks"][1]["play_list_vector"][0];
                REQUIRE (item.value ("tempo_locked", false));
            }
        }

        WHEN ("--tempo is combined with a ratio, or the lock value is garbage") {
            REQUIRE (cli::runClipSpeed (makeArgs ("clip-speed " + project.getFullPathName()
                                                  + " --region sine-0dB --tempo 60 --ratio 0.5"),
                                        context)
                     == cli::exitUsage);
            REQUIRE (cli::runClipSpeed (makeArgs ("clip-speed " + project.getFullPathName()
                                                  + " --region sine-0dB --lock-tempo maybe"),
                                        context)
                     == cli::exitUsage);
        }
    }

    workDir.deleteRecursively();
}

SCENARIO ("cli move-clip to a new track keeps the source panning", "[cli][region]")
{
    auto workDir = makeWorkDirectory();
    auto project = workDir.getChildFile ("pan.audium");

    // Written here rather than borrowed from TestFiles/, where the stem
    // separation tests generate a file of this name - relying on that made
    // this scenario depend on test order.
    auto audioFile = workDir.getChildFile ("stereo-saw.wav");
    {
        std::unique_ptr<juce::OutputStream> stream (audioFile.createOutputStream());
        REQUIRE (stream != nullptr);
        auto writer = juce::WavAudioFormat().createWriterFor (stream,
                                                              juce::AudioFormatWriter::Options{}
                                                                  .withSampleRate (44100.0)
                                                                  .withNumChannels (2)
                                                                  .withBitsPerSample (16));
        REQUIRE (writer != nullptr);
        juce::AudioBuffer<float> buffer (2, 44100);
        for (auto i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (0, i, (float) (i % 100) / 50.f - 1.f);
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
        REQUIRE (writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()));
    }
    REQUIRE (audioFile.existsAsFile());

    cli::CliContext context;
    context.quiet = true;

    auto projectArg = project.getFullPathName();
    auto pans = [] (const nlohmann::json& track) {
        std::vector<float> result;
        for (auto& channel : track.value ("channels", nlohmann::json::array()))
            result.push_back (channel.value ("pan", 0.f));
        return result;
    };

    GIVEN ("a stereo import, which is panned hard left and right") {
        REQUIRE (cli::runCreate (makeArgs ("create " + projectArg + " --channels 1"), context)
                 == cli::exitOk);
        REQUIRE (cli::runImport (makeArgs ("import " + projectArg + " " + audioFile.getFullPathName()),
                                 context)
                 == cli::exitOk);
        auto source = readProjectJson (project)["audium"]["audio_tracks"].back();
        REQUIRE (pans (source) == std::vector<float> { -1.f, 1.f });

        WHEN ("its clip is moved to a new track") {
            REQUIRE (cli::runMoveClip (makeArgs ("move-clip " + projectArg
                                                 + " --region stereo-saw --to-track new"),
                                       context)
                     == cli::exitOk);

            THEN ("the new track is panned the same way") {
                auto created = readProjectJson (project)["audium"]["audio_tracks"].back();
                REQUIRE (pans (created) == std::vector<float> { -1.f, 1.f });
            }
        }
    }

    workDir.deleteRecursively();
}

SCENARIO ("cli rejects out-of-range numeric options", "[cli][validation]")
{
    auto workDir = makeWorkDirectory();
    auto project = workDir.getChildFile ("validation.audium");
    auto outputFile = workDir.getChildFile ("bounce.wav");
    auto audioFile = juce::File (testFilesDir + "sine-0dB.wav");
    REQUIRE (audioFile.existsAsFile());

    const auto projectArg = project.getFullPathName();

    cli::CliContext context;
    context.quiet = true;

    nlohmann::json envelope;
    context.envelopeSink = [&envelope] (const nlohmann::json& e) { envelope = e; };

    GIVEN ("a project with one imported clip") {
        REQUIRE (cli::runCreate (makeArgs ("create " + projectArg + " --channels 1"), context) == cli::exitOk);
        REQUIRE (cli::runImport (makeArgs ("import " + projectArg + " " + audioFile.getFullPathName()), context)
                 == cli::exitOk);
        const auto before = readProjectJson (project);

        // each invocation must be a usage error in the standard envelope,
        // naming the option, and leave the project untouched
        auto requireUsageError = [&] (int exitCode, const std::string& option) {
            REQUIRE (exitCode == cli::exitUsage);
            REQUIRE (envelope["ok"] == false);
            REQUIRE (envelope["error"]["code"] == "usage");
            REQUIRE (envelope["error"]["message"].get<std::string>().find (option) != std::string::npos);
            REQUIRE (readProjectJson (project) == before);
        };

        WHEN ("import gets a negative or non-numeric --position") {
            const auto file = " " + audioFile.getFullPathName();
            requireUsageError (cli::runImport (makeArgs ("import " + projectArg + file + " --position -2"), context),
                               "--position");
            requireUsageError (cli::runImport (makeArgs ("import " + projectArg + file + " --position=abc"), context),
                               "--position");
            requireUsageError (cli::runImport (makeArgs ("import " + projectArg + file + " --position 1e"), context),
                               "--position");
            requireUsageError (cli::runImport (makeArgs ("import " + projectArg + file + " --position --1"), context),
                               "--position");
            requireUsageError (cli::runImport (makeArgs ("import " + projectArg + file + " --position +."), context),
                               "--position");
        }

        WHEN ("export gets a negative --start or a non-positive --length") {
            const auto out = " -o " + outputFile.getFullPathName();
            requireUsageError (cli::runExport (makeArgs ("export " + projectArg + out + " --start -1"), context),
                               "--start");
            requireUsageError (cli::runExport (makeArgs ("export " + projectArg + out + " --length 0"), context),
                               "--length");
            requireUsageError (cli::runExport (makeArgs ("export " + projectArg + out + " --length -3"), context),
                               "--length");
            REQUIRE_FALSE (outputFile.existsAsFile());
        }

        WHEN ("auto-edit and assemble get out-of-range values") {
            requireUsageError (cli::runAutoEdit (makeArgs ("auto-edit " + projectArg + " --duration -5"), context),
                               "--duration");
            requireUsageError (cli::runAutoEdit (makeArgs ("auto-edit " + projectArg + " --segments 0"), context),
                               "--segments");
            requireUsageError (cli::runAutoEdit (makeArgs ("auto-edit " + projectArg + " --measures -1"), context),
                               "--measures");
            requireUsageError (cli::runAssemble (makeArgs ("assemble " + projectArg + " --duration 0"), context),
                               "--duration");
        }

        WHEN ("export gets a valid zero --start") {
            REQUIRE (cli::runExport (makeArgs ("export " + projectArg + " -o " + outputFile.getFullPathName()
                                               + " --start 0 --length 0.5 --channels 1"),
                                     context)
                     == cli::exitOk);
            THEN ("it renders") {
                REQUIRE (envelope["ok"] == true);
                REQUIRE (outputFile.existsAsFile());
            }
        }
    }

    workDir.deleteRecursively();
}
