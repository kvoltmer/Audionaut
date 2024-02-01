/*
  ==============================================================================

    Streamable.h
    Created: 1 Feb 2024 1:40:08pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace audium
{

class Streamable
{

public:
    // TODO: documentation
    virtual bool writeToStream (juce::OutputStream& outputStream) = 0;
    // TODO: documentation
    virtual bool readFromStream (juce::InputStream& inputStream) = 0;
    // TODO: documentation
    virtual int getSizeInUnits() = 0;
};

} // namespace audium
