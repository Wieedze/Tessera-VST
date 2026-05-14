#pragma once

#include "IFxModule.h"

namespace tessera::dsp
{
    /**
     * @file TapeStopFx.h
     * @brief Tape-stop FX — variable-speed playback that decelerates from
     *        full speed (1.0) down to zero over a configurable cycle.
     *
     * Models the audible effect of cutting power to a tape machine: the
     * motor decelerates, pitch drops, sound elongates, then silence.
     *
     * In Tessera (no sequencer yet), the FX runs in a **looping** mode:
     * once the cycle reaches the stop, we re-anchor on the current writePos
     * of the CaptureBuffer and restart. With a 600 ms cycle this gives a
     * "tape ducking" feel that repeats musically.
     *
     * The read position uses a fractional phase accumulator, so the
     * deceleration is smooth (handled by CaptureBuffer::readInterpolated).
     *
     * @see docs/architecture-engines.md §3 (variable-speed playback)
     * @see docs/sprint-w3-plan.md §S3
     */
    class TapeStopFx : public IFxModule
    {
    public:
        TapeStopFx()           = default;
        ~TapeStopFx() override = default;

        void prepare (double sampleRate, int maxBlockSize) override;
        void reset() override;
        void process (juce::AudioBuffer<float>& buffer,
                      const CaptureBuffer&     capture,
                      const FxParams&          params) override;
        FxType getType() const override;

    private:
        double sampleRate { 0.0 };

        // State for the current deceleration cycle. anchored=false means we
        // need to capture a new startPos on the next sample (start of cycle).
        bool    anchored        { false };
        int64_t startPos        { 0 };
        double  phaseAccumulator { 0.0 };
        int     samplesPlayed   { 0 };

        /// Curve applied to the linear progress t in [0, 1].
        /// Returns the "remaining speed" factor (1.0 at t=0, 0.0 at t=1).
        static double computeSpeed (TapeCurve curve, double t) noexcept;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeStopFx)
    };
} // namespace tessera::dsp
