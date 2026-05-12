#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include "../../../source/dsp/fx/StutterFx.h"
#include "../../../source/dsp/fx/FxBank.h"

using namespace tessera::dsp;
using Catch::Matchers::WithinAbs;

namespace
{
    // Fills the capture buffer with a known ramp so we can verify what Stutter reads.
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

TEST_CASE ("StutterFx :: getType() returns Stutter", "[dsp][fx][stutter]")
{
    StutterFx fx;
    CHECK (fx.getType() == FxType::Stutter);
}

TEST_CASE ("StutterFx :: reset() puts playPos back to 0 (deterministic first-sample reads)",
           "[dsp][fx][stutter]")
{
    StutterFx fx;
    fx.prepare (48000.0, 64);

    CaptureBuffer capture;
    capture.prepare (48000.0);
    fillCaptureWithRamp (capture, 2, 10, 64);

    FxParams params;
    params.sampleRate  = 48000.0;
    params.bpm         = 120.0;
    params.stutterRate = 0.0625f; // 1/16

    juce::AudioBuffer<float> firstRun  (2, 64);
    juce::AudioBuffer<float> secondRun (2, 64);

    fx.reset();
    fx.process (firstRun, capture, params);

    fx.reset();
    fx.process (secondRun, capture, params);

    // After reset(), the very first sample of each channel reads from the SAME
    // position inside the loop window. So firstRun[ch][0] must match secondRun[ch][0].
    for (int ch = 0; ch < 2; ++ch)
        CHECK_THAT (firstRun.getSample (ch, 0),
                    WithinAbs (secondRun.getSample (ch, 0), 1e-6f));
}

TEST_CASE ("StutterFx :: produces non-silent output when capture has content",
           "[dsp][fx][stutter]")
{
    StutterFx fx;
    fx.prepare (48000.0, 64);

    CaptureBuffer capture;
    capture.prepare (48000.0);
    // At 120 BPM / 48k, a 1/16 stutter window is 6000 samples. We need to write
    // strictly more than that so the loop window points into non-zero territory.
    // 200 blocks of 64 = 12 800 samples, double the 1/16 window — safe margin.
    fillCaptureWithRamp (capture, 2, 200, 64);

    FxParams params;
    params.sampleRate  = 48000.0;
    params.bpm         = 120.0;
    params.stutterRate = 0.0625f; // 1/16 -> 6000 samples loop window

    juce::AudioBuffer<float> buffer (2, 64);
    fx.process (buffer, capture, params);

    // The ramp values inside [writePos - 6000, writePos) are 6800..12800-ish.
    // Some must surface in the output.
    bool anyNonZero = false;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i)
            if (std::abs (buffer.getSample (ch, i)) > 1e-6f)
                anyNonZero = true;
    CHECK (anyNonZero);
}

TEST_CASE ("StutterFx :: loop length depends on stutter rate", "[dsp][fx][stutter]")
{
    // Different rates -> different loopLength -> different loopStart -> different first read.
    StutterFx fx;
    fx.prepare (48000.0, 64);

    CaptureBuffer capture;
    capture.prepare (48000.0);
    fillCaptureWithRamp (capture, 2, 250, 64); // 16 000 samples available

    FxParams params;
    params.sampleRate = 48000.0;
    params.bpm        = 120.0;

    params.stutterRate = 0.0625f; // 1/16
    juce::AudioBuffer<float> out1over16 (2, 8);
    fx.reset();
    fx.process (out1over16, capture, params);

    params.stutterRate = 0.125f; // 1/8 (twice as long)
    juce::AudioBuffer<float> out1over8 (2, 8);
    fx.reset();
    fx.process (out1over8, capture, params);

    // The two first samples must differ — they come from different positions in the ramp.
    const float diff = std::abs (out1over16.getSample (0, 0) - out1over8.getSample (0, 0));
    CHECK (diff > 1.0f); // values are in 0..16000 range, a difference > 1.0 is comfortable
}

TEST_CASE ("StutterFx :: FxBank exposes StutterFx via get(FxType::Stutter)",
           "[dsp][fx][stutter][bank]")
{
    FxBank bank;
    bank.prepareAll (48000.0, 64);

    IFxModule& fx = bank.get (FxType::Stutter);
    CHECK (fx.getType() == FxType::Stutter);
}
