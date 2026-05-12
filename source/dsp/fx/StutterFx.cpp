#include "StutterFx.h"
#include <algorithm>

namespace tessera::dsp
{
    void StutterFx::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        juce::ignoreUnused (maxBlockSize);
        reset();
    }

    void StutterFx::reset()
    {
        playPos = 0;
    }

    void StutterFx::process (juce::AudioBuffer<float>& buffer,
                             const CaptureBuffer&     capture,
                             const FxParams&          params)
    {
        if (sampleRate <= 0.0 || params.bpm <= 0.0)
            return; // Not prepared, or invalid host info: leave buffer alone.

        // Compute the loop window length in samples from the musical rate.
        // stutterRate is a fraction of a whole note (0.0625 = 1/16, 0.125 = 1/8...).
        // duration_seconds = stutterRate * 240 / BPM  → samples = duration * sampleRate.
        const int loopLength = std::max (
            1,
            static_cast<int> (params.stutterRate * 240.0 / params.bpm * sampleRate));

        const int64_t writePos  = capture.getWritePos();
        const int64_t loopStart = writePos - loopLength;

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = std::min (buffer.getNumChannels(), 2);

        // Hot loop. RT-safe: no alloc, no lock, no exception path.
        for (int i = 0; i < numSamples; ++i)
        {
            const int    relativePos = playPos % loopLength;
            const double readPos     = static_cast<double> (loopStart + relativePos);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                buffer.setSample (ch, i, capture.readInterpolated (ch, readPos));
            }
            ++playPos;
        }
    }

    FxType StutterFx::getType() const
    {
        return FxType::Stutter;
    }
} // namespace tessera::dsp
