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
        anchored      = false;
        samplesPlayed = 0;
        playPos       = 0;
    }

    void StutterFx::process (juce::AudioBuffer<float>& buffer,
                             const CaptureBuffer&     capture,
                             const FxParams&          params)
    {
        if (sampleRate <= 0.0 || params.bpm <= 0.0)
            return; // Not prepared, or invalid host info: leave buffer alone.

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = std::min (buffer.getNumChannels(), 2);

        // Hot loop. RT-safe: no alloc, no lock, no exception path.
        for (int i = 0; i < numSamples; ++i)
        {
            // Re-anchor when:
            //   - we have not anchored yet (first call after reset()), OR
            //   - we have played a full loopLength of samples — refresh the
            //     anchor to the most recent slice. This gives a "tight stutter
            //     that follows the audio": each step is a true loop of the
            //     last loopLength ms, then we step to the next slice.
            //
            // duration_seconds = stutterRate * 240 / BPM  → samples = duration * sampleRate.
            if (! anchored || samplesPlayed >= anchoredLoopLength)
            {
                anchoredLoopLength = std::max (
                    1,
                    static_cast<int> (params.stutterRate * 240.0 / params.bpm * sampleRate));
                startPos      = capture.getWritePos() - anchoredLoopLength;
                samplesPlayed = 0;
                playPos       = 0;
                anchored      = true;
            }

            const double readPos = static_cast<double> (startPos + playPos);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                buffer.setSample (ch, i, capture.readInterpolated (ch, readPos));
            }
            ++playPos;
            ++samplesPlayed;
        }
    }

    FxType StutterFx::getType() const
    {
        return FxType::Stutter;
    }
} // namespace tessera::dsp
