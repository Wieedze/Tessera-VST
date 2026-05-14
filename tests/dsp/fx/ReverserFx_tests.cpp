#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include "../../../src/dsp/fx/ReverserFx.h"
#include "../../../src/dsp/fx/FxBank.h"

using namespace tessera::dsp;
using Catch::Matchers::WithinAbs;

namespace
{
    /// Fill the capture buffer with a deterministic ramp [0, 1, 2, ..., total-1]
    /// across both channels so we can assert exactly which sample was read.
    void fillCaptureWithRamp (CaptureBuffer& capture, int numChannels, int numBlocks, int blockSize)
    {
        juce::AudioBuffer<float> block (numChannels, blockSize);
        int counter = 0;
        for (int b = 0; b < numBlocks; ++b)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    block.setSample (ch, i, static_cast<float> (counter + i));
            capture.write (block);
            counter += blockSize;
        }
    }
}

TEST_CASE ("ReverserFx :: getType() returns Reverser", "[dsp][fx][reverser]")
{
    ReverserFx fx;
    CHECK (fx.getType() == FxType::Reverser);
}

TEST_CASE ("ReverserFx :: reset() puts playPos back to 0 (deterministic first read)",
           "[dsp][fx][reverser]")
{
    ReverserFx fx;
    fx.prepare (48000.0, 64);

    CaptureBuffer capture;
    capture.prepare (48000.0);
    // 200 blocks * 64 = 12 800 samples ; window of 250 ms = 12 000 samples → fully populated.
    fillCaptureWithRamp (capture, 2, 200, 64);

    FxParams params;
    params.sampleRate       = 48000.0;
    params.reverserWindowMs = 250.0f;

    juce::AudioBuffer<float> firstRun  (2, 64);
    juce::AudioBuffer<float> secondRun (2, 64);

    fx.reset();
    fx.process (firstRun, capture, params);

    fx.reset();
    fx.process (secondRun, capture, params);

    // After reset(), the very first sample reads from the SAME position
    // inside the window. firstRun[ch][0] must match secondRun[ch][0].
    for (int ch = 0; ch < 2; ++ch)
        CHECK_THAT (firstRun.getSample (ch, 0),
                    WithinAbs (secondRun.getSample (ch, 0), 1e-6f));
}

TEST_CASE ("ReverserFx :: produces non-silent output when capture has content",
           "[dsp][fx][reverser]")
{
    ReverserFx fx;
    fx.prepare (48000.0, 64);

    CaptureBuffer capture;
    capture.prepare (48000.0);
    fillCaptureWithRamp (capture, 2, 200, 64); // 12 800 samples

    FxParams params;
    params.sampleRate       = 48000.0;
    params.reverserWindowMs = 250.0f; // 12 000 samples window

    juce::AudioBuffer<float> buffer (2, 64);
    fx.process (buffer, capture, params);

    // The window covers ramp positions [800, 12800) — all non-zero values.
    bool anyNonZero = false;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i)
            if (std::abs (buffer.getSample (ch, i)) > 1e-6f)
                anyNonZero = true;
    CHECK (anyNonZero);
}

TEST_CASE ("ReverserFx :: output decreases over the block (reverse playback property)",
           "[dsp][fx][reverser]")
{
    // The CaptureBuffer holds a monotonically increasing ramp. When we read it
    // in reverse, the output values inside one block must monotonically DECREASE
    // as i goes from 0 to numSamples-1 (until we wrap around the window end).
    // We pick a small block (32 samples) inside a large window (12 000 samples)
    // so no wrap happens — strict monotonic decrease.

    ReverserFx fx;
    fx.prepare (48000.0, 64);

    CaptureBuffer capture;
    capture.prepare (48000.0);
    fillCaptureWithRamp (capture, 2, 200, 64);

    FxParams params;
    params.sampleRate       = 48000.0;
    params.reverserWindowMs = 250.0f;

    juce::AudioBuffer<float> buffer (2, 32);
    fx.reset();
    fx.process (buffer, capture, params);

    // ch 0 only — both channels carry the same ramp, one check is enough.
    for (int i = 1; i < 32; ++i)
    {
        const float prev = buffer.getSample (0, i - 1);
        const float curr = buffer.getSample (0, i);
        CHECK (curr <= prev);
    }
}

TEST_CASE ("ReverserFx :: FxBank exposes ReverserFx via get(FxType::Reverser)",
           "[dsp][fx][reverser][bank]")
{
    FxBank bank;
    bank.prepareAll (48000.0, 64);

    IFxModule& fx = bank.get (FxType::Reverser);
    CHECK (fx.getType() == FxType::Reverser);
}
