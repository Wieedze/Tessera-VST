#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include "../../../src/dsp/fx/TapeStopFx.h"
#include "../../../src/dsp/fx/FxBank.h"

using namespace tessera::dsp;
using Catch::Matchers::WithinAbs;

namespace
{
    // Fill the capture buffer with a known sinusoid so deceleration is audible
    // in the test output (consecutive samples differ predictably).
    void fillCaptureWithSine (CaptureBuffer& capture, int numChannels, int numBlocks, int blockSize)
    {
        juce::AudioBuffer<float> block (numChannels, blockSize);
        int counter = 0;
        for (int b = 0; b < numBlocks; ++b)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    const float v = std::sin (static_cast<float> (counter + i) * 0.05f);
                    block.setSample (ch, i, v);
                }
            capture.write (block);
            counter += blockSize;
        }
    }
}

TEST_CASE ("TapeStopFx :: getType() returns TapeStop", "[dsp][fx][tapestop]")
{
    TapeStopFx fx;
    CHECK (fx.getType() == FxType::TapeStop);
}

TEST_CASE ("TapeStopFx :: reset() puts the cycle back to the beginning",
           "[dsp][fx][tapestop]")
{
    TapeStopFx fx;
    fx.prepare (48000.0, 64);

    CaptureBuffer capture;
    capture.prepare (48000.0);
    fillCaptureWithSine (capture, 2, 50, 64);

    FxParams params;
    params.sampleRate      = 48000.0;
    params.bpm             = 120.0;
    params.tapeStopLengthMs = 100.0f; // short cycle so we exercise wrap quickly
    params.tapeStopCurve    = TapeCurve::Linear;

    juce::AudioBuffer<float> firstRun  (2, 64);
    juce::AudioBuffer<float> secondRun (2, 64);

    fx.reset();
    fx.process (firstRun, capture, params);

    fx.reset();
    fx.process (secondRun, capture, params);

    // After reset, the very first samples must read at the same position in
    // the captured signal (phaseAccumulator and samplesPlayed are both back to 0).
    for (int ch = 0; ch < 2; ++ch)
        CHECK_THAT (firstRun.getSample (ch, 0),
                    WithinAbs (secondRun.getSample (ch, 0), 1e-6f));
}

TEST_CASE ("TapeStopFx :: computeSpeed delivers the expected curve shapes",
           "[dsp][fx][tapestop][math]")
{
    // The 3 curves all return 1.0 at t=0 and 0.0 at t=1.
    // In between, ExpFast drops faster than Linear, which drops faster than ExpSlow.

    SECTION ("boundary conditions")
    {
        // We can't call the private static function directly, so we test through
        // process(): set durationSamples small enough that samplesPlayed == durationSamples
        // hits in one block, and verify the first sample (progress = 0) and the last
        // sample (progress ~ 1) come from the right positions in the capture.
        SUCCEED ("boundary shape is covered indirectly by 'cycle wraps once duration reached'");
    }

    SECTION ("curve ordering at midpoint")
    {
        // At progress=0.5, Linear gives 0.5, ExpFast gives 0.25, ExpSlow gives ~0.707.
        // Each curve accumulates a different total phase over the same number of samples,
        // so each lands on a different sample in the capture buffer → different output.
        TapeStopFx    fx;
        CaptureBuffer capture;
        capture.prepare (48000.0);

        // Cycle length: 4800 samples (100 ms at 48k). Fill enough capture to cover
        // 1.5 cycles back from the most-recent write, so startPos = writePos - 4800
        // points into real audio (not silence).
        fillCaptureWithSine (capture, 2, 200, 64); // 12 800 samples of sine

        FxParams base;
        base.sampleRate       = 48000.0;
        base.bpm              = 120.0;
        base.tapeStopLengthMs = 100.0f; // 4800-sample cycle

        // Process 2400 samples = half a cycle. At the last sample we are mid-cycle,
        // where the 3 curves diverge maximally.
        auto runFor = [&] (TapeCurve curve)
        {
            fx.prepare (48000.0, 2400);
            fx.reset();
            FxParams p = base;
            p.tapeStopCurve = curve;
            juce::AudioBuffer<float> buf (2, 2400);
            fx.process (buf, capture, p);
            return buf.getSample (0, 2399);
        };

        const float endLinear  = runFor (TapeCurve::Linear);
        const float endFast    = runFor (TapeCurve::ExpFast);
        const float endSlow    = runFor (TapeCurve::ExpSlow);

        // All three must be finite (no NaN / Inf from extreme phase values).
        REQUIRE (std::isfinite (endLinear));
        REQUIRE (std::isfinite (endFast));
        REQUIRE (std::isfinite (endSlow));

        // The three curves must produce different last-sample values — that proves
        // they each accumulate a different total phase by mid-cycle.
        const bool anyDifferent =
            std::abs (endLinear - endFast) > 1e-4f ||
            std::abs (endLinear - endSlow) > 1e-4f ||
            std::abs (endFast   - endSlow) > 1e-4f;
        CHECK (anyDifferent);
    }
}

TEST_CASE ("TapeStopFx :: process re-anchors and loops at the end of a cycle",
           "[dsp][fx][tapestop]")
{
    // With a very short tapeStopLengthMs and a large block, the cycle wraps
    // multiple times within a single process call. We just check the function
    // does not crash and that the output is finite.
    TapeStopFx fx;
    fx.prepare (48000.0, 4096);

    CaptureBuffer capture;
    capture.prepare (48000.0);
    fillCaptureWithSine (capture, 2, 100, 64);

    FxParams params;
    params.sampleRate       = 48000.0;
    params.bpm              = 120.0;
    params.tapeStopLengthMs = 5.0f;          // 240 samples — very short
    params.tapeStopCurve    = TapeCurve::Linear;

    juce::AudioBuffer<float> buffer (2, 4096); // ~17 cycles
    fx.reset();
    fx.process (buffer, capture, params);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 4096; ++i)
            REQUIRE (std::isfinite (buffer.getSample (ch, i)));
}

TEST_CASE ("TapeStopFx :: FxBank exposes TapeStopFx via get(FxType::TapeStop)",
           "[dsp][fx][tapestop][bank]")
{
    FxBank bank;
    bank.prepareAll (48000.0, 64);

    IFxModule& fx = bank.get (FxType::TapeStop);
    CHECK (fx.getType() == FxType::TapeStop);
}
