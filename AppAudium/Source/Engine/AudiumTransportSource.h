/*
  ==============================================================================

    AudiumTransportSource.h
    Created: 14 Jun 2023 6:12:34pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

class AudioResourceContainer;

class AudiumTransportSource : public juce::AudioTransportSource
{
    
    
public:
    AudiumTransportSource(std::shared_ptr<AudioResourceContainer> audioResourceContainer) :
        audioResourceContainer(audioResourceContainer)
    {
    }
    
    ~AudiumTransportSource() override
    {
    }
    
    //==============================================================================
    /** Changes the current playback position in the source stream.

        The next time the getNextAudioBlock() method is called, this
        is the time from which it'll read data.

        @param newPosition    the new playback position in seconds

        @see getCurrentPosition
    */
    void setPosition (double newPosition);

    /** Returns the position that the next data block will be read from
        This is a time in seconds.
    */
    double getCurrentPosition() const;
    
    //==============================================================================
    /** Starts playing (if a source has been selected).

        If it starts playing, this will send a message to any ChangeListeners
        that are registered with this object.
    */
    void start();

    /** Stops playing.

        If it's actually playing, this will send a message to any ChangeListeners
        that are registered with this object.
    */
    void stop();

    /** Returns true if it's currently playing. */
    bool isPlaying() const;
    
    
private:
    
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    
};
