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

class AutoEdit {
    
public:
    AutoEdit(std::shared_ptr<AudioResourceContainer> audioResourceContainer,
             std::shared_ptr<AudioRegionContainer> audioRegionContainer,
             std::shared_ptr<PlayListContainer> playListContainer);
    
    bool invokeAutoEdit();
    void applyAutoEditResult();
    
private:
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<AudioRegionContainer> audioRegionContainer;
    std::shared_ptr<PlayListContainer> playListContainer;
    std::string audioResourceFilePath;
    
    const std::string getBaseName() const;
    const std::string getCountFromFile() const;
};
