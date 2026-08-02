//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <iostream>

#include "AnalysisCache.h"

namespace audium {

const char* AnalysisCache::fileName = "AnalysisData.json";

std::string analysisTypeToString(AnalysisType analysisType)
{
    switch (analysisType) {
        case AnalysisType::SBic:       return "sbic";
        case AnalysisType::Onset:      return "onset";
        case AnalysisType::Beat:       return "beat";
        case AnalysisType::BeatDegara: return "beat_degara";
    }
    jassertfalse;
    return "unknown";
}

std::optional<AnalysisType> analysisTypeFromString(const std::string& name)
{
    if (name == "sbic")        return AnalysisType::SBic;
    if (name == "onset")       return AnalysisType::Onset;
    if (name == "beat")        return AnalysisType::Beat;
    if (name == "beat_degara") return AnalysisType::BeatDegara;
    return std::nullopt;
}

std::string AnalysisCache::makeKey(const juce::String& path,
                                   juce::int64 size,
                                   juce::int64 modificationTime,
                                   AnalysisType analysisType)
{
    // Combine the analysis type with the file identity (path + size +
    // modification time) so that a changed file yields a different key and
    // therefore misses any stale entry.
    juce::String key;
    key << (int) analysisType << "|"
        << path << "|"
        << juce::String(size) << "|"
        << juce::String(modificationTime);

    return key.toStdString();
}

std::string AnalysisCache::makeKey(const juce::File& audioFile,
                                   AnalysisType analysisType)
{
    return makeKey(audioFile.getFullPathName(),
                   audioFile.getSize(),
                   audioFile.getLastModificationTime().toMilliseconds(),
                   analysisType);
}

std::optional<std::vector<float>> AnalysisCache::get(const juce::File& audioFile,
                                                     AnalysisType analysisType) const
{
    const std::lock_guard<std::mutex> lock(mutex);

    const auto it = entries.find(makeKey(audioFile, analysisType));
    if (it == entries.end())
        return std::nullopt;

    return it->second.segments;
}

std::optional<float> AnalysisCache::getBpm(const juce::File& audioFile,
                                           AnalysisType analysisType) const
{
    const std::lock_guard<std::mutex> lock(mutex);

    const auto it = entries.find(makeKey(audioFile, analysisType));
    if (it == entries.end())
        return std::nullopt;

    return it->second.bpm;
}

std::unordered_map<std::string, std::vector<float>>
    AnalysisCache::getAll(AnalysisType analysisType) const
{
    const std::lock_guard<std::mutex> lock(mutex);

    std::unordered_map<std::string, std::vector<float>> result;
    for (const auto& [key, entry] : entries)
        if (entry.type == analysisType)
            result[entry.path] = entry.segments;

    return result;
}

void AnalysisCache::put(const juce::File& audioFile,
                        AnalysisType analysisType,
                        std::vector<float> result,
                        float bpm)
{
    const std::lock_guard<std::mutex> lock(mutex);

    Entry entry;
    entry.type = analysisType;
    entry.path = audioFile.getFullPathName().toStdString();
    entry.size = audioFile.getSize();
    entry.modificationTime = audioFile.getLastModificationTime().toMilliseconds();
    entry.segments = std::move(result);
    entry.bpm = bpm;

    entries[makeKey(audioFile, analysisType)] = std::move(entry);
}

void AnalysisCache::rebaseAudioFolder(const juce::File& newAudioFolder)
{
    const std::lock_guard<std::mutex> lock(mutex);

    // Re-keying changes the map keys, so build a fresh map.
    std::unordered_map<std::string, Entry> rebased;
    rebased.reserve(entries.size());

    for (auto& [key, entry] : entries) {
        const auto newFile = newAudioFolder.getChildFile(juce::File(entry.path).getFileName());
        if (newFile.existsAsFile()) {
            entry.path = newFile.getFullPathName().toStdString();
            entry.size = newFile.getSize();
            entry.modificationTime = newFile.getLastModificationTime().toMilliseconds();
        }

        auto newKey = makeKey(juce::String(entry.path), entry.size,
                              entry.modificationTime, entry.type);
        rebased[std::move(newKey)] = std::move(entry);
    }

    entries = std::move(rebased);
}

void AnalysisCache::clear()
{
    const std::lock_guard<std::mutex> lock(mutex);
    entries.clear();
}

size_t AnalysisCache::size() const
{
    const std::lock_guard<std::mutex> lock(mutex);
    return entries.size();
}

bool AnalysisCache::writeToJson(json& output)
{
    const std::lock_guard<std::mutex> lock(mutex);

    output["version"] = dataVersion;

    const bool relative = serializationFolder != juce::File();

    auto records = json::array();
    for (const auto& [key, entry] : entries) {
        // Store paths relative to the project folder so the sidecar survives
        // the whole package being moved on disk.
        const auto storedPath = relative
            ? juce::File(entry.path).getRelativePathFrom(serializationFolder)
            : juce::String(entry.path);

        json record;
        record["type"] = analysisTypeToString(entry.type);
        record["path"] = storedPath.toStdString();
        record["size"] = entry.size;
        record["mtime"] = entry.modificationTime;
        record["segments"] = entry.segments;
        record["bpm"] = entry.bpm;
        records.push_back(std::move(record));
    }
    output["entries"] = std::move(records);

    return true;
}

bool AnalysisCache::readFromJson(json& input, bool /*rebuild*/)
{
    const std::lock_guard<std::mutex> lock(mutex);

    entries.clear();

    // Results written by another version describe analyses that no longer
    // produce them, so they are dropped rather than trusted - see dataVersion.
    const auto version = input.value("version", 0);

    if (version != dataVersion)
    {
        std::cout << "AnalysisCache: discarding analysis data written by version "
                  << version << " (this build expects " << dataVersion
                  << "); affected files will be analysed again." << std::endl;
        return true;
    }

    if (! input.contains("entries") || ! input["entries"].is_array())
        return true;

    const bool relative = serializationFolder != juce::File();

    for (const auto& record : input["entries"]) {
        if (! record.contains("type") || ! record.contains("path")
            || ! record.contains("segments"))
            continue;

        const auto type = analysisTypeFromString(record["type"].get<std::string>());
        if (! type.has_value())
            continue; // unknown/future analysis type - skip gracefully

        // Resolve the stored (relative) path against the current project folder
        // so the entry points at the file's new location if the package moved.
        const auto storedPath = juce::String(record["path"].get<std::string>());
        const auto absolutePath = relative
            ? serializationFolder.getChildFile(storedPath).getFullPathName()
            : storedPath;

        Entry entry;
        entry.type = *type;
        entry.path = absolutePath.toStdString();
        entry.size = record.value("size", (juce::int64) 0);
        entry.modificationTime = record.value("mtime", (juce::int64) 0);
        entry.segments = record["segments"].get<std::vector<float>>();
        entry.bpm = record.value("bpm", 0.0f);

        // Reconstruct the identity key from the resolved path + stored fields so
        // an unchanged file hits on a later get().
        auto key = makeKey(absolutePath, entry.size,
                           entry.modificationTime, entry.type);
        entries[std::move(key)] = std::move(entry);
    }

    return true;
}

int AnalysisCache::getSizeInUnits()
{
    const std::lock_guard<std::mutex> lock(mutex);
    return static_cast<int>(entries.size());
}

bool AnalysisCache::saveToFolder(const juce::File& projectFolder)
{
    // Nothing to persist - don't create an empty sidecar.
    if (size() == 0)
        return true;

    if (! projectFolder.isDirectory())
        return false;

    const auto file = projectFolder.getChildFile(fileName);

    // Serialise entry paths relative to the project folder.
    serializationFolder = projectFolder;

    bool wrote = false;
    juce::TemporaryFile temp(file);
    if (auto out = std::unique_ptr<juce::FileOutputStream>(temp.getFile().createOutputStream())) {
        if (! out->failedToOpen() && writeToStream(*out)) {
            out->flush();
            wrote = true;
        }
    }
    // The output stream is closed here (out went out of scope) before we swap
    // the temporary file into place.
    const bool success = wrote && temp.overwriteTargetFileWithTemporary();

    serializationFolder = juce::File();
    return success;
}

bool AnalysisCache::loadFromFolder(const juce::File& projectFolder)
{
    clear();

    const auto file = projectFolder.getChildFile(fileName);
    if (! file.existsAsFile() || file.getSize() == 0)
        return true; // no (usable) sidecar - project simply has no analysis data

    juce::FileInputStream in(file);
    if (! in.openedOk())
        return false;

    // Resolve stored relative paths against the current project folder.
    serializationFolder = projectFolder;
    const bool success = readFromStream(in);
    serializationFolder = juce::File();
    return success;
}

} // namespace audium
