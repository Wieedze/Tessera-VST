#pragma once

#include "IFxModule.h"

namespace tessera::dsp
{
    /**
     * @file ThruFx.h
     * @brief Bypass FX — input passes through unchanged.
     *
     * Used by the Sequencer when a step is disabled or when its probability
     * roll falls below threshold. Also proves the IFxModule contract works:
     * a stateless, do-nothing implementation that still plugs into FxBank.
     *
     * @see docs/architecture-engines.md §3
     */
    class ThruFx : public IFxModule
    {
    public:
        ThruFx()           = default;
        ~ThruFx() override = default;

        void prepare (double sampleRate, int maxBlockSize) override;
        void reset() override;
        void process (juce::AudioBuffer<float>& buffer,
                      const CaptureBuffer&     capture,
                      const FxParams&          params) override;
        FxType getType() const override;

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThruFx)
    };
} // namespace tessera::dsp
