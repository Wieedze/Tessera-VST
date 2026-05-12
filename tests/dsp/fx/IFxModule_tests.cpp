#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../../../source/dsp/fx/IFxModule.h"

using namespace tessera::dsp;
using Catch::Matchers::WithinAbs;

TEST_CASE ("IFxModule :: header compiles and types are available", "[dsp][fx][ifxmodule]")
{
    // FxType is reachable.
    constexpr FxType t = FxType::Thru;
    static_assert (t == FxType::Thru);

    // FxParams is a plain struct, default-constructible, with the documented defaults.
    // Float comparisons use WithinAbs to avoid -Wfloat-equal even on exact-literal cases.
    FxParams p;
    CHECK_THAT (p.sampleRate,   WithinAbs (48000.0,  1e-9));
    CHECK_THAT (p.bpm,          WithinAbs (120.0,    1e-9));
    CHECK_THAT (p.stutterRate,  WithinAbs (0.0625f,  1e-6f));
    CHECK_THAT (p.stutterDecay, WithinAbs (0.0f,     1e-6f));
    CHECK_THAT (p.stutterGate,  WithinAbs (1.0f,     1e-6f));

    // IFxModule cannot be instantiated directly — it is abstract.
    // The following would NOT compile (we leave it as a comment to make the point):
    // tessera::dsp::IFxModule m;  // error: cannot allocate an object of abstract type
}
