//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>

namespace audium {


class AudiumEngine;
class PlayListContainer;
class AudioTrackContainer;
class PlayListItem;

struct AutoEditConfig {
    std::string mode = "random";
    double duration = 120.0;
    int numSegments = 20;
    double minSegLength = 2.0;
    double maxSegLength = 60.0;
    std::string bounceFileName = "";
    int trackId = -1;
    int playlistItemId = -1;
};

class AutoEdit {
    
public:
    AutoEdit(std::shared_ptr<AudiumEngine> audiumEngine_) :
        audiumEngine(audiumEngine_)
    {}
    
    bool invokeAutoEdit(AutoEditConfig &config,
                        std::function<void(std::string)> callback);
    
    bool invokePython(AutoEditConfig &config,
                      std::function<void(std::string)> callback);
    
    void applyAutoEditResult(double sampleRate);
    
    bool createRegionsFromSegFile(std::string segFileName, double sampleRate);
    bool createPlayListFromSongFile(std::string songFileName);
    
    static const juce::String getTempDirectory();
    
private:
    std::shared_ptr<AudiumEngine> audiumEngine;
    
    std::string audioResourceFilePath;
    
    const std::string getBaseName() const;
    const std::string getCountFromFile() const;
};

} // namespace audium

