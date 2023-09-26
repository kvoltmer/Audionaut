/*
  ==============================================================================

    AudioResourceGroup.h
    Created: 26 Sep 2023 11:22:03am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

class AudioResourceGroup {
    
    
public:
    AudioResourceGroup(std::string nameString) :
        name(nameString)
    {}
    
    const std::string getName() const { return name; }
    
private:
    std::string name;
    
};
