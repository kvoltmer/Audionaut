/*
  ==============================================================================

    AutoEdit.h
    Created: 14 Sep 2023 3:13:12pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <memory>

class AudioResourceContainer;
class AudioRegionContainer;
class PlayListContainer;
class AudioGroupContainer;

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
    AutoEdit(std::shared_ptr<AudioGroupContainer> audioGroupContainer,
             std::shared_ptr<AudioResourceContainer> audioResourceContainer) :
        audioGroupContainer(audioGroupContainer),
        audioResourceContainer(audioResourceContainer)
    {}
    
    bool invokeAutoEdit(const AutoEditConfig config);
    void applyAutoEditResult(double sampleRate);
    
    bool createRegionsFromSegFile(std::string segFileName, double sampleRate);
    bool createPlayListFromSongFile(std::string songFileName);
    
    static const juce::String getTempDirectory();
    
private:
    std::shared_ptr<AudioGroupContainer> audioGroupContainer;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    
    std::string audioResourceFilePath;
    
    const std::string getBaseName() const;
    const std::string getCountFromFile() const;
};
