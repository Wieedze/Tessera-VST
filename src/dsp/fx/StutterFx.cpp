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
        anchored = false;
        playPos  = 0;
    }

    void StutterFx::process (juce::AudioBuffer<float>& buffer,
                             const CaptureBuffer&     capture,
                             const FxParams&          params)
    {
        if (sampleRate <= 0.0 || params.bpm <= 0.0)
            return; // Not prepared, or invalid host info: leave buffer alone.

        // First call after reset(): anchor the loop window once and LOCK
        // loopLength. Subsequent calls reuse the same anchor, so the same
        // slice of audio loops repeatedly (true stutter, not a sliding delay).
        // duration_seconds = stutterRate * 240 / BPM  → samples = duration * sampleRate.
        if (! anchored)
        {
            anchoredLoopLength = std::max (
                1,
                static_cast<int> (params.stutterRate * 240.0 / params.bpm * sampleRate));
            startPos = capture.getWritePos() - anchoredLoopLength;
            anchored = true;
        }

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = std::min (buffer.getNumChannels(), 2);

        // Hot loop. RT-safe: no alloc, no lock, no exception path.
        for (int i = 0; i < numSamples; ++i)
        {
            const int    relativePos = playPos % anchoredLoopLength;
            const double readPos     = static_cast<double> (startPos + relativePos);

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
