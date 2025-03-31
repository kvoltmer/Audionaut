//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#include "TransportLoop.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/Provider/TempoProvider.h"

namespace audium {

void TransportLoop::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    externalSampleRate = sampleRate;
}

void TransportLoop::setLoopPositionRange(std::shared_ptr<AudioTrackContainer> audioTrackContainer,
                                         juce::Range<double> newRange,
                                         audium::TimeContextType context)
{
    if (newRange.getStart() >= 0.0) {
        
        auto minLength = loopData.minimumLoopLengthClocks;
        if (context == audium::seconds)
            minLength = tempoProvider->clocksToSeconds(loopData.minimumLoopLengthClocks);
        
        if (newRange.getLength() >= minLength) {
            
            // undo
            auto action = std::make_unique<audium::UndoableContainerAction>(*audioTrackContainer.get(), false);
            
            if (context == audium::seconds)
                newRange = tempoProvider->secondsToClocks(newRange);
            
            if (loopData.loopActive) {
                if (loopCount > 0) {
                    auto oldRange = getLoopPositionRange(audium::clocks);
                    auto loopDuration = loopCount * oldRange.getLength();
                    loopCount = loopDuration / newRange.getLength();
                }
            }
            loopData.loopStartPositionClocks = newRange.getStart();
            loopData.loopEndPositionClocks = newRange.getEnd();
            
            
            // undo
            action->storeNewState();
            undoManager->perform(action.release(), "Change Loop");
            undoManager->beginNewTransaction();
        }
    }
    else {
        std::cout << "setLoopPositionRange invalid range: " << newRange.getStart() << " " << newRange.getEnd() << std::endl;
    }
    
}

juce::Range<double> TransportLoop::getLoopPositionRange(audium::TimeContextType context) const
{
    juce::Range<double> range(loopData.loopStartPositionClocks,
                              loopData.loopEndPositionClocks);
    if (context == audium::clocks) {
        return range;
    }
    else if (context == audium::seconds) {
        return tempoProvider->clocksToSeconds(range);
    }
    
    return juce::Range<double>(0.0, 0.0);
}

bool TransportLoop::isLoopActive() const
{
    return loopData.loopActive;
}

void TransportLoop::setLoopActive(bool bActive)
{
    loopData.loopActive = bActive;
}


bool TransportLoop::processLoop(double &thePosition, int numSamples)
{
    auto loopResult = false;
    
    auto loopRange = getLoopPositionRange(audium::clocks);
    
    auto clocksThisBuffer = tempoProvider->secondsToClocks(static_cast<double>(numSamples) / externalSampleRate);
    
    // subtract previous loops
    thePosition -= (static_cast<double>(loopCount) * loopRange.getLength());
    jassert(thePosition >= 0.0);
    
    if (loopData.loopActive) {
        
        // loop event
        if (withinLoop &&
            thePosition + clocksThisBuffer > loopRange.getEnd()) {
            
            thePosition -= loopRange.getLength();
            loopCount++;
            loopResult = true;
        }
        else if (loopRange.contains(thePosition)) {
            withinLoop = true;
        }
        else {
            withinLoop = false;
        }
    }
    else {
        withinLoop = false;
    }
    
    return loopResult;
}

void TransportLoop::reset()
{
    loopCount = 0;
}

} // namespace audium
