#pragma once

#include "IFxModule.h"

namespace tessera::dsp
{
    /**
     * @file ReverserFx.h
     * @brief Reverser FX — replays the last N ms of the capture buffer in reverse.
     *
     * Each block, re-anchors the window to the current writePos and reads
     * backwards: when playPos advances 0, 1, 2, ..., the position inside the
     * window steps (windowLength - 1), (windowLength - 2), ..., 0. This is
     * the classic "symmetric reflection" of an index — N-1-i mirrors i around
     * the centre of the window.
     *
     * No feedback, no internal filter — a stateless transform on top of the
     * shared CaptureBuffer. Simpler than StutterFx because the output sample
     * order is purely a function of (writePos, windowLength, playPos).
     *
     * @see docs/architecture-engines.md §3
     */
    class ReverserFx : public IFxModule
    {
    public:
        ReverserFx()           = default;
        ~ReverserFx() override = default;

        void prepare (double sampleRate, int maxBlockSize) override;
        void reset() override;
        void process (juce::AudioBuffer<float>& buffer,
                      const CaptureBuffer&     capture,
                      const FxParams&          params) override;
        FxType getType() const override;

    private:
        double sampleRate { 0.0 }; // captured at prepare()
        int    playPos    { 0 };   // loops inside [0, windowLength); reset at reset()

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverserFx)
    };
} // namespace tessera::dsp
