//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AnalysisCache.h"

namespace audium {

std::string AnalysisCache::makeKey(const juce::File& audioFile,
                                   AnalysisType analysisType)
{
    // Combine the analysis type with the file identity (path + size +
    // modification time) so that a changed file yields a different key and
    // therefore misses any stale entry.
    juce::String key;
    key << (int) analysisType << "|"
        << audioFile.getFullPathName() << "|"
        << juce::String(audioFile.getSize()) << "|"
        << juce::String(audioFile.getLastModificationTime().toMilliseconds());

    return key.toStdString();
}

std::optional<std::vector<float>> AnalysisCache::get(const juce::File& audioFile,
                                                     AnalysisType analysisType) const
{
    const std::lock_guard<std::mutex> lock(mutex);

    const auto it = entries.find(makeKey(audioFile, analysisType));
    if (it == entries.end())
        return std::nullopt;

    return it->second;
}

void AnalysisCache::put(const juce::File& audioFile,
                        AnalysisType analysisType,
                        std::vector<float> result)
{
    const std::lock_guard<std::mutex> lock(mutex);
    entries[makeKey(audioFile, analysisType)] = std::move(result);
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

} // namespace audium
