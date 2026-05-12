#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../../src/dsp/CaptureBuffer.h"

using Catch::Matchers::WithinAbs;

TEST_CASE ("CaptureBuffer :: default-constructs without allocating", "[dsp][capturebuffer]")
{
    tessera::dsp::CaptureBuffer cb;

    CHECK (cb.getBufferSize() == 0);
    CHECK (cb.getWritePos()   == 0);
    CHECK (cb.isFrozen()      == false);
}

TEST_CASE ("CaptureBuffer :: prepare() allocates and resets state", "[dsp][capturebuffer][prepare]")
{
    tessera::dsp::CaptureBuffer cb;

    SECTION ("48 kHz allocates 32 seconds worth of samples")
    {
        cb.prepare (48000.0);
        CHECK (cb.getBufferSize() == 48000 * 32);
    }

    SECTION ("96 kHz allocates 32 seconds worth of samples")
    {
        cb.prepare (96000.0);
        CHECK (cb.getBufferSize() == 96000 * 32);
    }

    SECTION ("prepare() resets writePos and freeze flag")
    {
        cb.freeze (true);
        cb.prepare (48000.0);
        CHECK (cb.getWritePos() == 0);
        CHECK (cb.isFrozen()    == false);
    }
}

TEST_CASE ("CaptureBuffer :: write() advances writePos and respects freeze", "[dsp][capturebuffer][write]")
{
    tessera::dsp::CaptureBuffer cb;
    cb.prepare (48000.0);

    // Build a small stereo input block, filled with a known ramp.
    juce::AudioBuffer<float> input (2, 64);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i)
            input.setSample (ch, i, static_cast<float> (i));

    SECTION ("writePos advances by numSamples after one write")
    {
        cb.write (input);
        CHECK (cb.getWritePos() == 64);
    }

    SECTION ("writePos accumulates across multiple writes")
    {
        cb.write (input);
        cb.write (input);
        cb.write (input);
        CHECK (cb.getWritePos() == 64 * 3);
    }

    SECTION ("freeze() makes write() a no-op")
    {
        cb.write (input);
        const auto posBefore = cb.getWritePos();

        cb.freeze (true);
        cb.write (input);

        CHECK (cb.getWritePos() == posBefore); // unchanged
    }

    SECTION ("freeze(false) re-enables writing")
    {
        cb.freeze (true);
        cb.write (input);
        CHECK (cb.getWritePos() == 0); // still zero, the write was blocked

        cb.freeze (false);
        cb.write (input);
        CHECK (cb.getWritePos() == 64);
    }
}

TEST_CASE ("CaptureBuffer :: readInterpolated() round-trip and interpolation", "[dsp][capturebuffer][read]")
{
    tessera::dsp::CaptureBuffer cb;
    cb.prepare (48000.0);

    // Build a known input: ch 0 ramp [0,1,2,...,63], ch 1 ramp doubled [0,2,4,...]
    juce::AudioBuffer<float> input (2, 64);
    for (int i = 0; i < 64; ++i)
    {
        input.setSample (0, i, static_cast<float> (i));
        input.setSample (1, i, static_cast<float> (i * 2));
    }
    cb.write (input);

    SECTION ("integer positions return exact samples")
    {
        // After writing samples 0..63, buffer[ch0][0] == 0, [ch0][1] == 1, etc.
        CHECK_THAT (cb.readInterpolated (0,  0.0), WithinAbs (0.0f,  1e-6f));
        CHECK_THAT (cb.readInterpolated (0, 10.0), WithinAbs (10.0f, 1e-6f));
        CHECK_THAT (cb.readInterpolated (0, 63.0), WithinAbs (63.0f, 1e-6f));

        CHECK_THAT (cb.readInterpolated (1,  5.0), WithinAbs (10.0f, 1e-6f)); // 5*2
    }

    SECTION ("fractional positions linearly interpolate between neighbours")
    {
        // At pos = 10.5, halfway between sample[10]=10 and sample[11]=11 → 10.5
        CHECK_THAT (cb.readInterpolated (0, 10.5),  WithinAbs (10.5f,  1e-6f));

        // At pos = 10.25, 25% of the way: 10 + 0.25*(11-10) = 10.25
        CHECK_THAT (cb.readInterpolated (0, 10.25), WithinAbs (10.25f, 1e-6f));

        // At pos = 10.75: 10 + 0.75*1 = 10.75
        CHECK_THAT (cb.readInterpolated (0, 10.75), WithinAbs (10.75f, 1e-6f));
    }

    SECTION ("read on empty buffer (not prepared) returns silence")
    {
        tessera::dsp::CaptureBuffer empty;
        CHECK_THAT (empty.readInterpolated (0, 0.0), WithinAbs (0.0f, 1e-6f));
        CHECK_THAT (empty.readInterpolated (1, 5.5), WithinAbs (0.0f, 1e-6f));
    }

    SECTION ("read on out-of-range channel returns silence")
    {
        CHECK_THAT (cb.readInterpolated (-1, 10.0), WithinAbs (0.0f, 1e-6f));
        CHECK_THAT (cb.readInterpolated ( 5, 10.0), WithinAbs (0.0f, 1e-6f));
    }
}

TEST_CASE ("CaptureBuffer :: RT-safety stress (no crash, no NaN over many calls)", "[dsp][capturebuffer][rt-safety]")
{
    tessera::dsp::CaptureBuffer cb;
    cb.prepare (48000.0);

    juce::AudioBuffer<float> block (2, 512);
    juce::Random rng;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            block.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f); // full-scale random

    constexpr int iterations = 2000; // ~10 seconds of audio at 48k/512

    SECTION ("write() does not crash or leak under sustained calls")
    {
        for (int n = 0; n < iterations; ++n)
            cb.write (block);

        // After 2000 writes of 512 samples, writePos = 1 024 000.
        CHECK (cb.getWritePos() == iterations * 512);
    }

    SECTION ("readInterpolated() does not produce NaN/Inf across the full buffer")
    {
        for (int n = 0; n < iterations; ++n)
            cb.write (block);

        for (int i = 0; i < 1000; ++i)
        {
            const double pos = rng.nextFloat() * static_cast<float> (cb.getBufferSize());
            const float  v0  = cb.readInterpolated (0, pos);
            const float  v1  = cb.readInterpolated (1, pos);
            REQUIRE (std::isfinite (v0));
            REQUIRE (std::isfinite (v1));
            REQUIRE (v0 >= -1.0f);
            REQUIRE (v0 <=  1.0f);
            REQUIRE (v1 >= -1.0f);
            REQUIRE (v1 <=  1.0f);
        }
    }
}

TEST_CASE ("CaptureBuffer :: wrap-around when writePos exceeds bufferSize", "[dsp][capturebuffer][wrap]")
{
    tessera::dsp::CaptureBuffer cb;
    cb.prepare (48000.0); // bufferSize = 48000 * 32 = 1 536 000

    // Write a single distinct value at position 0, then fill 1.5 buffers worth
    // of writes to wrap around. The original value should still be retrievable
    // because the ring buffer overwrote index 0 with the latest content.
    juce::AudioBuffer<float> input (2, 1024);
    input.clear();
    input.setSample (0, 0, 999.0f); // marker

    cb.write (input);                            // writePos = 1024, buf[0]=999
    CHECK_THAT (cb.readInterpolated (0, 0.0), WithinAbs (999.0f, 1e-6f));

    // After enough silent writes, writePos > bufferSize, idx 0 holds silence.
    // We'll write enough to wrap once.
    input.clear();
    const int total = cb.getBufferSize() + 100;
    int written     = 1024;
    while (written < total)
    {
        cb.write (input);
        written += 1024;
    }

    // writePos has wrapped at least once; sample at original "absolute pos 0"
    // (= modulo index 0 in the ring) has been overwritten with zero.
    CHECK_THAT (cb.readInterpolated (0, 0.0), WithinAbs (0.0f, 1e-6f));
    // And writePos still keeps growing past bufferSize — never wraps itself.
    CHECK (cb.getWritePos() > cb.getBufferSize());
}
