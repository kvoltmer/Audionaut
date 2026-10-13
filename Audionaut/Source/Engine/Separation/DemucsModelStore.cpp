//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Engine/Separation/DemucsModelStore.h"

namespace audium {

namespace {

// The ggml conversion of Meta's htdemucs (4 sources) published alongside
// demucs.cpp. Pinned to a repository revision rather than "main" so the
// checksum below cannot drift under a re-upload.
const char* const modelRevision = "8f58ac0491bbea657275bcd3e38af1ab3a27bfc9";
const char* const modelFileName = "ggml-model-htdemucs-4s-f16.bin";

} // namespace

const DemucsModelInfo& DemucsModelStore::defaultModel()
{
    static const DemucsModelInfo info = []
    {
        DemucsModelInfo model;
        model.fileName = modelFileName;
        model.url = juce::URL ("https://huggingface.co/datasets/Retrobear/demucs.cpp/resolve/"
                               + juce::String (modelRevision) + "/" + modelFileName);
        model.fallbackUrl = juce::URL ("https://audionaut.app/models/" + juce::String (modelFileName));
        model.sha256Hex = "72b17c42d308982ddb5069bc3bf48b81a5aac4cb6516e4366c0fa7cef6df0064";
        model.expectedBytes = 83994361;
        return model;
    }();

    return info;
}

juce::File DemucsModelStore::defaultDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Audionaut")
               .getChildFile ("Models");
}

DemucsModelStore DemucsModelStore::createDefault()
{
    return DemucsModelStore (defaultDirectory());
}

DemucsModelStore::DemucsModelStore (juce::File directory_) :
    directory (std::move (directory_)),
    model (defaultModel())
{
}

void DemucsModelStore::setModel (DemucsModelInfo info)
{
    model = std::move (info);
}

juce::File DemucsModelStore::getModelFile() const
{
    return directory.getChildFile (model.fileName);
}

bool DemucsModelStore::isAvailable() const
{
    const auto file = getModelFile();
    return file.existsAsFile() && file.getSize() == model.expectedBytes;
}

bool DemucsModelStore::verify (const juce::File& file, juce::String& error) const
{
    if (! file.existsAsFile())
    {
        error = "file not found: " + file.getFullPathName();
        return false;
    }

    if (model.expectedBytes > 0 && file.getSize() != model.expectedBytes)
    {
        error = "unexpected size: " + juce::String (file.getSize()) + " bytes, expected "
                + juce::String (model.expectedBytes);
        return false;
    }

    const auto digest = juce::SHA256 (file).toHexString();

    if (! digest.equalsIgnoreCase (model.sha256Hex))
    {
        error = "checksum mismatch: the file is not the expected model";
        return false;
    }

    return true;
}

bool DemucsModelStore::moveIntoPlace (const juce::File& candidate, juce::String& error)
{
    if (! verify (candidate, error))
    {
        candidate.deleteFile();
        return false;
    }

    const auto target = getModelFile();
    target.deleteFile();

    if (! candidate.moveFileTo (target))
    {
        candidate.deleteFile();
        error = "could not move the model into " + directory.getFullPathName();
        return false;
    }

    return true;
}

bool DemucsModelStore::download (const DownloadProgress& progress, juce::String& error)
{
    error.clear();

    // The primary host first, the mirror when it cannot deliver. A
    // cancellation stops the whole download - only failures fall through.
    for (const auto& url : { model.url, model.fallbackUrl })
    {
        if (url.isEmpty())
            continue;

        auto cancelled = false;
        juce::String attemptError;

        if (downloadFrom (url, progress, cancelled, attemptError))
            return true;

        if (cancelled)
            return false;

        error = error.isEmpty() ? attemptError : error + "; " + attemptError;
    }

    if (error.isEmpty())
        error = "the model could not be downloaded";

    return false;
}

bool DemucsModelStore::downloadFrom (const juce::URL& url,
                                     const DownloadProgress& progress,
                                     bool& cancelled,
                                     juce::String& error)
{
    cancelled = false;

    if (! directory.createDirectory())
    {
        error = "could not create " + directory.getFullPathName();
        return false;
    }

    const auto partFile = getModelFile().withFileExtension ("part");
    partFile.deleteFile();

    // Hugging Face answers with a redirect to its CDN.
    auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                       .withConnectionTimeoutMs (30000)
                       .withNumRedirectsToFollow (10);

    std::unique_ptr<juce::InputStream> input (url.createInputStream (options));

    if (input == nullptr)
    {
        error = "could not connect to " + url.getDomain();
        return false;
    }

    std::unique_ptr<juce::FileOutputStream> output (partFile.createOutputStream());

    if (output == nullptr || output->failedToOpen())
    {
        error = "could not write to " + partFile.getFullPathName();
        return false;
    }

    const auto total = input->getTotalLength();
    juce::int64 done = 0;
    juce::HeapBlock<char> buffer (1 << 16);

    while (! input->isExhausted())
    {
        const auto read = input->read (buffer.getData(), 1 << 16);

        if (read < 0)
        {
            output.reset();
            partFile.deleteFile();
            error = "the download was interrupted";
            return false;
        }

        if (read > 0)
        {
            if (! output->write (buffer.getData(), static_cast<size_t> (read)))
            {
                output.reset();
                partFile.deleteFile();
                error = "could not write to " + partFile.getFullPathName();
                return false;
            }

            done += read;
        }

        if (progress != nullptr && ! progress (done, total))
        {
            output.reset();
            partFile.deleteFile();
            cancelled = true;
            return false;
        }

        if (read == 0)
            break;
    }

    output.reset();

    return moveIntoPlace (partFile, error);
}

bool DemucsModelStore::installFromFile (const juce::File& source, juce::String& error)
{
    if (! verify (source, error))
        return false;

    if (! directory.createDirectory())
    {
        error = "could not create " + directory.getFullPathName();
        return false;
    }

    const auto partFile = getModelFile().withFileExtension ("part");
    partFile.deleteFile();

    if (! source.copyFileTo (partFile))
    {
        error = "could not copy " + source.getFullPathName();
        return false;
    }

    return moveIntoPlace (partFile, error);
}

bool DemucsModelStore::remove()
{
    getModelFile().withFileExtension ("part").deleteFile();
    return getModelFile().deleteFile();
}

} // namespace audium
