//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <memory>

namespace audium {


class AudioResourceContainer;
class AudioRegionContainer;
class PlayListContainer;
class AudioTrackContainer;

struct AutoEditConfig {
    std::string mode = "random";
    double duration = 120.0;
    int numSegments = 20;
    double minSegLength = 2.0;
    double maxSegLength = 60.0;
    std::string bounceFileName = "";
};

class AutoEdit {
    
public:
    AutoEdit(std::shared_ptr<AudioTrackContainer> audioTrackContainer,
             std::shared_ptr<AudioResourceContainer> audioResourceContainer) :
    audioTrackContainer(audioTrackContainer),
    audioResourceContainer(audioResourceContainer)
    {}
    
    bool invokeAutoEdit(const AutoEditConfig config);
    void applyAutoEditResult(double sampleRate);
    
    bool createRegionsFromSegFile(std::string segFileName, double sampleRate);
    bool createPlayListFromSongFile(std::string songFileName);
    
    static const juce::String getTempDirectory();
    
private:
    std::shared_ptr<AudioTrackContainer> audioTrackContainer;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    
    std::string audioResourceFilePath;
    
    const std::string getBaseName() const;
    const std::string getCountFromFile() const;
};

} // namespace audium

