#include "TapeStopFx.h"
#include <algorithm>
#include <cmath>

namespace tessera::dsp
{
    double TapeStopFx::computeSpeed (TapeCurve curve, double t) noexcept
    {
        // Clamp t into [0, 1] defensively. Above 1.0 would give negative speed.
        const double tt = std::clamp (t, 0.0, 1.0);
        const double remaining = 1.0 - tt;

        switch (curve)
        {
            case TapeCurve::Linear:  return remaining;
            case TapeCurve::ExpFast: return remaining * remaining;          // (1-t)^2
            case TapeCurve::ExpSlow: return std::sqrt (remaining);          // sqrt(1-t)
            case TapeCurve::Count:
            default:                 return remaining;
        }
    }

    void TapeStopFx::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        juce::ignoreUnused (maxBlockSize);
        reset();
    }

    void TapeStopFx::reset()
    {
        anchored         = false;
        startPos         = 0;
        phaseAccumulator = 0.0;
        samplesPlayed    = 0;
    }

    void TapeStopFx::process (juce::AudioBuffer<float>& buffer,
                              const CaptureBuffer&     capture,
                              const FxParams&          params)
    {
        if (sampleRate <= 0.0)
            return; // not prepared

        const double durationSamples = std::max (
            1.0,
            static_cast<double> (params.tapeStopLengthMs) * 0.001 * sampleRate);

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = std::min (buffer.getNumChannels(), 2);

        for (int i = 0; i < numSamples; ++i)
        {
            // Start of a new cycle: re-anchor in the PAST (writePos - durationSamples)
            // and read forward through the already-captured audio. With a decelerating
            // speed, the total phase accumulated over durationSamples is strictly less
            // than durationSamples, so we never read past writePos.
            //
            // Anchoring at writePos (the "future" boundary) would read silence —
            // there is no audio there yet.
            if (! anchored || static_cast<double> (samplesPlayed) >= durationSamples)
            {
                startPos         = capture.getWritePos() - static_cast<int64_t> (durationSamples);
                phaseAccumulator = 0.0;
                samplesPlayed    = 0;
                anchored         = true;
            }

            const double progress = static_cast<double> (samplesPlayed) / durationSamples;
            const double speed    = computeSpeed (params.tapeStopCurve, progress);

            const double readPos = static_cast<double> (startPos) + phaseAccumulator;

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, i, capture.readInterpolated (ch, readPos));

            phaseAccumulator += speed;
            ++samplesPlayed;
        }
    }

    FxType TapeStopFx::getType() const
    {
        return FxType::TapeStop;
    }
} // namespace tessera::dsp
