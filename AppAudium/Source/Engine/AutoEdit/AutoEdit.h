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

struct AutoEditConfig {
    std::string mode = "random";
    double duration = 60.0;
    int numSegments = 20;
    double minSegLength = 2.0;
    double maxSegLength = 60.0;
};

class AutoEdit {
    
public:
    AutoEdit(std::shared_ptr<AudioResourceContainer> audioResourceContainer,
             std::shared_ptr<AudioRegionContainer> audioRegionContainer,
             std::shared_ptr<PlayListContainer> playListContainer);
    
    bool invokeAutoEdit(const AutoEditConfig config);
    void applyAutoEditResult();
    
private:
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<AudioRegionContainer> audioRegionContainer;
    std::shared_ptr<PlayListContainer> playListContainer;
    std::string audioResourceFilePath;
    
    const std::string getBaseName() const;
    const std::string getCountFromFile() const;
};
