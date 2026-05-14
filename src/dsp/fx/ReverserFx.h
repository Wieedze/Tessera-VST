#pragma once

#include "IFxModule.h"

namespace tessera::dsp
{
    /**
     * @file ReverserFx.h
     * @brief Reverser FX — replays a fixed slice of the capture buffer in reverse.
     *
     * Trigger semantic (one-shot anchor) :
     * - reset() is called when the host (PluginProcessor) detects the user
     *   switching INTO Reverser. anchored is set to false.
     * - On the first process() call after reset(), we anchor: startPos =
     *   writePos - windowLength and LOCK windowLength. Subsequent process()
     *   calls keep the same anchor, so the SAME slice of audio is reversed
     *   over and over — a real reverse loop, not a sliding-window delay.
     *
     * Symmetric reflection: when playPos advances 0, 1, 2, ..., the position
     * inside the window steps (windowLength - 1), (windowLength - 2), ..., 0.
     * The math is the classic mirror N-1-i.
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
        double  sampleRate           { 0.0 }; // captured at prepare()
        bool    anchored             { false }; // set true on first process() after reset()
        int64_t startPos             { 0 };   // absolute write-position anchor for the current window
        int     anchoredWindowLength { 1 };   // window length in samples, locked at first anchor
        int     samplesPlayed        { 0 };   // counts samples since last (re)anchor; triggers re-anchor when >= windowLength
        int     playPos              { 0 };   // position inside the current window (0..anchoredWindowLength-1)

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverserFx)
    };
} // namespace tessera::dsp
