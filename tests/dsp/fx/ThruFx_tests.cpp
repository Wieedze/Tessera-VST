#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <memory>
#include "../../../src/dsp/fx/ThruFx.h"

using namespace tessera::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE ("ThruFx :: getType() returns Thru", "[dsp][fx][thru]")
{
    ThruFx fx;
    CHECK (fx.getType() == FxType::Thru);
}

TEST_CASE ("ThruFx :: process() leaves the buffer unchanged", "[dsp][fx][thru]")
{
    ThruFx        fx;
    CaptureBuffer capture;
    FxParams      params;
    fx.prepare      (48000.0, 64);
    capture.prepare (48000.0);

    // Build a small stereo buffer with a known ramp.
    juce::AudioBuffer<float> buffer (2, 64);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i)
            buffer.setSample (ch, i, static_cast<float> (ch * 100 + i));

    // Snapshot expected values.
    std::array<float, 128> expected {};
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i)
            expected[ch * 64 + i] = buffer.getSample (ch, i);

    fx.process (buffer, capture, params);

    // Verify nothing changed.
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i)
            CHECK_THAT (buffer.getSample (ch, i),
                        WithinAbs (expected[ch * 64 + i], 1e-6f));
}

TEST_CASE ("ThruFx :: works through IFxModule base pointer (polymorphism)",
           "[dsp][fx][thru][polymorphism]")
{
    // Own a ThruFx by an IFxModule* — the very pattern FxBank will use.
    std::unique_ptr<IFxModule> fx = std::make_unique<ThruFx>();
    CaptureBuffer              capture;
    FxParams                   params;
    fx->prepare      (48000.0, 64);
    capture.prepare  (48000.0);

    CHECK (fx->getType() == FxType::Thru);

    juce::AudioBuffer<float> buffer (2, 8);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 8; ++i)
            buffer.setSample (ch, i, 0.5f);

    fx->process (buffer, capture, params); // virtual dispatch → ThruFx::process

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 8; ++i)
            CHECK_THAT (buffer.getSample (ch, i), WithinAbs (0.5f, 1e-6f));

    // unique_ptr will delete via IFxModule* — virtual destructor ensures ~ThruFx runs.
}
