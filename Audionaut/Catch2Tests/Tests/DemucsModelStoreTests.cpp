//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>

#include "Engine/Separation/DemucsModelStore.h"

// The model store's install, verify and remove paths against a small stand-in
// file. Nothing here touches the network.

using namespace audium;

namespace {

struct TempStore {
    juce::File directory;
    DemucsModelStore store;

    TempStore() :
        directory (juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("audionaut-model-store-test").getNonexistentSibling()),
        store (directory)
    {
    }

    ~TempStore() { directory.deleteRecursively(); }
};

juce::File writeStandIn (const juce::String& content)
{
    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("audionaut-model-standin.bin").getNonexistentSibling();
    REQUIRE (file.replaceWithText (content));
    return file;
}

DemucsModelInfo describe (const juce::File& file)
{
    DemucsModelInfo info;
    info.fileName = "stand-in-model.bin";
    info.sha256Hex = juce::SHA256 (file).toHexString();
    info.expectedBytes = file.getSize();
    return info;
}

} // namespace

SCENARIO ("the model store installs only files matching the expected checksum", "[separation][model]")
{
    TempStore temp;
    auto genuine = writeStandIn ("these are the weights");
    auto impostor = writeStandIn ("those are the weights");   // same length: size alone must not pass

    temp.store.setModel (describe (genuine));

    REQUIRE_FALSE (temp.store.isAvailable());
    REQUIRE (temp.store.getModelFile() == temp.directory.getChildFile ("stand-in-model.bin"));

    WHEN ("a file with the wrong checksum is installed")
    {
        juce::String error;
        REQUIRE_FALSE (temp.store.installFromFile (impostor, error));

        THEN ("the store stays empty and says why")
        {
            REQUIRE (error.contains ("checksum"));
            REQUIRE_FALSE (temp.store.isAvailable());
            REQUIRE_FALSE (temp.store.getModelFile().exists());
            REQUIRE_FALSE (temp.store.getModelFile().withFileExtension ("part").exists());
        }
    }

    WHEN ("a file with the wrong size is installed")
    {
        auto tooShort = writeStandIn ("weights");
        juce::String error;
        REQUIRE_FALSE (temp.store.installFromFile (tooShort, error));
        REQUIRE (error.contains ("size"));
        tooShort.deleteFile();
    }

    WHEN ("the genuine file is installed")
    {
        juce::String error;
        REQUIRE (temp.store.installFromFile (genuine, error));

        THEN ("the model is available, verified in place, and the source is untouched")
        {
            REQUIRE (error.isEmpty());
            REQUIRE (temp.store.isAvailable());
            REQUIRE (temp.store.verify (temp.store.getModelFile(), error));
            REQUIRE (genuine.existsAsFile());
        }

        THEN ("removing it empties the store again")
        {
            REQUIRE (temp.store.remove());
            REQUIRE_FALSE (temp.store.isAvailable());
        }
    }

    WHEN ("a missing file is verified")
    {
        juce::String error;
        REQUIRE_FALSE (temp.store.verify (temp.directory.getChildFile ("nope.bin"), error));
        REQUIRE (error.contains ("not found"));
    }

    genuine.deleteFile();
    impostor.deleteFile();
}

SCENARIO ("the default model description is the pinned htdemucs download", "[separation][model]")
{
    const auto& model = DemucsModelStore::defaultModel();

    REQUIRE (model.fileName == "ggml-model-htdemucs-4s-f16.bin");
    REQUIRE (model.url.getScheme() == "https");
    REQUIRE (model.url.toString (false).contains ("huggingface.co"));
    REQUIRE_FALSE (model.url.toString (false).contains ("/main/"));   // pinned to a revision
    REQUIRE (model.sha256Hex.length() == 64);
    REQUIRE (model.expectedBytes > 80 * 1024 * 1024);

    REQUIRE (DemucsModelStore::defaultDirectory().getFileName() == "Models");
    REQUIRE (DemucsModelStore::createDefault().getModelFile().getFileName() == model.fileName);
}

// Hidden: fetches the real 84 MB model. Run with AudionautTests "[download]".
SCENARIO ("the model store downloads and verifies the pinned model", "[.][download]")
{
    TempStore temp;
    juce::String error;
    juce::int64 lastTotal = -1;

    const auto ok = temp.store.download ([&lastTotal] (juce::int64, juce::int64 total)
    {
        lastTotal = total;
        return true;
    }, error);

    INFO ("error: " << error);
    REQUIRE (ok);
    REQUIRE (temp.store.isAvailable());
    REQUIRE (temp.store.verify (temp.store.getModelFile(), error));
    REQUIRE_FALSE (temp.store.getModelFile().withFileExtension ("part").exists());
    REQUIRE (lastTotal == temp.store.getModel().expectedBytes);
}
