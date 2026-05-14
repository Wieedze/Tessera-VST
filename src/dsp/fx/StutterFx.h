#pragma once

#include "IFxModule.h"

namespace tessera::dsp
{
    /**
     * @file StutterFx.h
     * @brief Stutter / repeat FX — loops a fixed slice of the capture buffer.
     *
     * Trigger semantic (one-shot anchor) :
     * - reset() is called when the host (PluginProcessor) detects the user
     *   switching INTO Stutter from another FX. anchored is set to false.
     * - On the first process() call after reset(), we anchor: startPos =
     *   writePos - loopLength, and we LOCK the loopLength derived from the
     *   stutter_rate at that instant.
     * - Subsequent process() calls keep the same anchor: playPos advances and
     *   wraps modulo loopLength, so the SAME slice of audio loops repeatedly.
     *   This is a real stutter, not a sliding-window delay.
     *
     * Caveat — the CaptureBuffer keeps being written by other process() blocks,
     * so after roughly bufferSize samples (~32 s at 48 kHz) the anchored slice
     * gets overwritten. Acceptable for short musical bursts; a future polish
     * may freeze() the capture for the duration.
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
        double  sampleRate         { 0.0 }; // captured at prepare()
        bool    anchored           { false }; // set true on first process() after reset()
        int64_t startPos           { 0 };   // absolute write-position anchor for the current loop
        int     anchoredLoopLength { 1 };   // loop length in samples, locked at first anchor
        int     samplesPlayed      { 0 };   // counts samples since last (re)anchor; triggers re-anchor when >= loopLength
        int     playPos            { 0 };   // position inside the current loop (0..anchoredLoopLength-1)

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StutterFx)
    };
} // namespace tessera::dsp
