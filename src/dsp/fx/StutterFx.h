#pragma once

#include "IFxModule.h"

namespace tessera::dsp
{
    /**
     * @file StutterFx.h
     * @brief Stutter / repeat FX — loops the last N samples of the capture buffer.
     *
     * On each process() call, captures the current writePos of the CaptureBuffer,
     * walks backwards by loopLength samples (derived from the musical stutter rate
     * and host BPM), and re-reads that window in a tight loop. The window is
     * recomputed every block, giving the "live repeating" feel.
     *
     * @see docs/architecture-engines.md §3 (StutterFx example)
     */
    class StutterFx : public IFxModule
    {
    public:
        StutterFx()           = default;
        ~StutterFx() override = default;

        void prepare (double sampleRate, int maxBlockSize) override;
        void reset() override;
        void process (juce::AudioBuffer<float>& buffer,
                      const CaptureBuffer&     capture,
                      const FxParams&          params) override;
        FxType getType() const override;

    private:
        double sampleRate { 0.0 };  // captured at prepare()
        int    playPos    { 0 };    // loops inside [0, loopLength); reset at reset()

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StutterFx)
    };
} // namespace tessera::dsp
