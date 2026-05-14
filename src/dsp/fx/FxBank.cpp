#include "FxBank.h"
#include "ThruFx.h"
#include "StutterFx.h"
#include "ReverserFx.h"

namespace tessera::dsp
{
    FxBank::FxBank()
    {
        // Pre-allocate every concrete FX. Order does not matter; we index by FxType.
        // Each new FX type adds exactly one make_unique<...> line here.
        modules[static_cast<size_t> (FxType::Thru)]     = std::make_unique<ThruFx>();     // RT-OK: constructor body
        modules[static_cast<size_t> (FxType::Stutter)]  = std::make_unique<StutterFx>();  // RT-OK: constructor body
        modules[static_cast<size_t> (FxType::Reverser)] = std::make_unique<ReverserFx>(); // RT-OK: constructor body
    }

    void FxBank::prepareAll (double sampleRate, int maxBlockSize)
    {
        for (auto& mod : modules)
            if (mod)
                mod->prepare (sampleRate, maxBlockSize);
    }

    void FxBank::resetAll()
    {
        for (auto& mod : modules)
            if (mod)
                mod->reset();
    }

    IFxModule& FxBank::get (FxType type)
    {
        const auto idx = static_cast<size_t> (type);
        jassert (idx < modules.size() && "FxType out of range");
        jassert (modules[idx] != nullptr && "FxType not yet implemented in FxBank");
        return *modules[idx];
    }
} // namespace tessera::dsp
