#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../../../src/dsp/fx/FxBank.h"

using namespace tessera::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE ("FxBank :: default construction populates Thru", "[dsp][fx][bank]")
{
    FxBank bank;

    // The Thru slot must be filled at construction time.
    IFxModule& thru = bank.get (FxType::Thru);
    CHECK (thru.getType() == FxType::Thru);
}

TEST_CASE ("FxBank :: prepareAll and resetAll do not crash", "[dsp][fx][bank]")
{
    FxBank bank;
    bank.prepareAll (48000.0, 64);
    bank.resetAll();
    SUCCEED ("no crash");
}

TEST_CASE ("FxBank :: get() returns a reference that processes through IFxModule polymorphism",
           "[dsp][fx][bank][polymorphism]")
{
    FxBank        bank;
    CaptureBuffer capture;
    FxParams      params;

    bank.prepareAll (48000.0, 64);
    capture.prepare (48000.0);

    juce::AudioBuffer<float> buffer (2, 16);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 16; ++i)
            buffer.setSample (ch, i, static_cast<float> (i + 1));

    // get() returns IFxModule& — virtual dispatch will route to ThruFx::process.
    IFxModule& fx = bank.get (FxType::Thru);
    fx.process (buffer, capture, params);

    // Bypass behavior — samples unchanged.
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 16; ++i)
            CHECK_THAT (buffer.getSample (ch, i),
                        WithinAbs (static_cast<float> (i + 1), 1e-6f));
}
