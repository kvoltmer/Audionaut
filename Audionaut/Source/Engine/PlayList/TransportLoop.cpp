//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

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
            std::unique_ptr<UndoableContainerAction> action = nullptr;
            if (audioTrackContainer != nullptr)
                action = std::make_unique<audium::UndoableContainerAction>(*audioTrackContainer.get(), false);
            
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
            if (action != nullptr &&
                undoManager != nullptr) {
                action->storeNewState();
                undoManager->perform(action.release(), "Change Loop");
                undoManager->beginNewTransaction();
            }
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
    virtualPosition = thePosition;
    
    auto loopEvent = false;
    
    auto loopRange = getLoopPositionRange(audium::clocks);
    
    jassert(externalSampleRate > 0.0);
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
            loopEvent = true;
        }
        else if (loopRange.contains(thePosition)) {
            if (not withinLoop) {
                withinLoop = true;
                tempoProvider->sendActionMessage(audium::transportLoopEntered);
            }
        }
        else {
            withinLoop = false;
        }
    }
    else {
        withinLoop = false;
    }
    
    if (loopEvent) {
        // async message
        tempoProvider->sendActionMessage(audium::transportLoopAction);
    }
    currentPosition = thePosition;
    return loopEvent;
}

void TransportLoop::reset()
{
    loopCount = 0;
    withinLoop = false;
}

void TransportLoop::setAbsoluteStartPosition(double newPosition, audium::TimeContextType context)
{
    auto positionClocks = 0.0;
    if (context == audium::clocks) {
        positionClocks = newPosition;
    }
    else if (context == audium::seconds) {
        positionClocks = tempoProvider->secondsToClocks(newPosition);
    }
    
    auto loopRange = getLoopPositionRange(audium::clocks);
    
    if (loopRange.contains(positionClocks)) {
        withinLoop = true;
    }
    
}

double TransportLoop::getCurrentPosition(audium::TimeContextType context) const noexcept
{
    if (context == audium::clocks) {
        return currentPosition;
    }
    else if (context == audium::seconds) {
        return tempoProvider->secondsToClocks(currentPosition);
    }
    jassertfalse;
    return 0.0;
}

double TransportLoop::getLoopPhaseForPosition(double startPosition,
                                              double duration,
                                              audium::TimeContextType context) const
{
    if (context == audium::seconds) {
        startPosition = tempoProvider->secondsToClocks(startPosition);
        duration = tempoProvider->secondsToClocks(duration);
    }
    auto loopRange = getLoopPositionRange(context);
    if (loopRange.getLength() > 0.0) {
        auto durationInLoop = startPosition + duration - loopRange.getStart();
        return durationInLoop / loopRange.getLength();
    }
    jassertfalse;
    return 0.0;
}

} // namespace audium
