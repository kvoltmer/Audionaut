//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <cstdint>

#include <JuceHeader.h>

namespace audium
{

/**
 * Where the transport is for the block being rendered: playing or not, the
 * position of the block's first sample, the tempo and the loop. One per
 * engine. The scheduler publishes it before it hands a block to the
 * renderer - in the device callback and in a bounce alike, so a bounce
 * reports the position of the block it renders, not the transport's of the
 * session - and whatever runs inside the block reads it: a hosted plugin
 * asks for it through its playhead.
 *
 * Threads: publish() from the one thread that drives the render, before the
 * block's jobs start; read() from any thread. Each value is an atomic of its
 * own, so a read from inside the block gets the block's values, and a read
 * from elsewhere (a plugin asking from its window) may get values of two
 * neighbouring blocks, each of them valid.
 */
class RenderTransport
{
public:
    struct Block
    {
        bool playing = false;
        std::int64_t positionSamples = 0;   ///< the block's first sample, at the render's sample rate
        double positionSeconds = 0.0;
        double positionBeats = 0.0;         ///< quarter notes from the project's start
        double tempo = 120.0;               ///< beats per minute
        bool looping = false;
        double loopStartBeats = 0.0;
        double loopEndBeats = 0.0;
    };

    RenderTransport() = default;

    void publish (const Block& block) noexcept
    {
        playing.store (block.playing, std::memory_order_relaxed);
        positionSamples.store (block.positionSamples, std::memory_order_relaxed);
        positionSeconds.store (block.positionSeconds, std::memory_order_relaxed);
        positionBeats.store (block.positionBeats, std::memory_order_relaxed);
        tempo.store (block.tempo, std::memory_order_relaxed);
        looping.store (block.looping, std::memory_order_relaxed);
        loopStartBeats.store (block.loopStartBeats, std::memory_order_relaxed);
        loopEndBeats.store (block.loopEndBeats, std::memory_order_release);
    }

    /// The tempo alone, from the thread that sets it: what an effect that keeps time by it goes by
    /// before a render has published a block (a project just opened, with no device running).
    void publishTempo (double beatsPerMinute) noexcept
    {
        tempo.store (beatsPerMinute, std::memory_order_relaxed);
    }

    Block read() const noexcept
    {
        Block block;
        block.loopEndBeats = loopEndBeats.load (std::memory_order_acquire);
        block.loopStartBeats = loopStartBeats.load (std::memory_order_relaxed);
        block.looping = looping.load (std::memory_order_relaxed);
        block.tempo = tempo.load (std::memory_order_relaxed);
        block.positionBeats = positionBeats.load (std::memory_order_relaxed);
        block.positionSeconds = positionSeconds.load (std::memory_order_relaxed);
        block.positionSamples = positionSamples.load (std::memory_order_relaxed);
        block.playing = playing.load (std::memory_order_relaxed);
        return block;
    }

private:
    std::atomic<bool> playing { false };
    std::atomic<std::int64_t> positionSamples { 0 };
    std::atomic<double> positionSeconds { 0.0 };
    std::atomic<double> positionBeats { 0.0 };
    std::atomic<double> tempo { 120.0 };
    std::atomic<bool> looping { false };
    std::atomic<double> loopStartBeats { 0.0 };
    std::atomic<double> loopEndBeats { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RenderTransport)
};

} // namespace audium
