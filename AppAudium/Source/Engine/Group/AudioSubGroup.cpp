/*
  ==============================================================================

    AudioSubGroup.cpp
    Created: 19 Dec 2023 3:47:13pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioSubGroup.h"

bool AudioSubGroup::writeToStream (juce::OutputStream& outputStream)
{
    outputStream.writeInt(subGroupId);
    return true;
}

bool AudioSubGroup::readFromStream (juce::InputStream& inputStream)
{
    subGroupId = inputStream.readInt();
    return true;
}
