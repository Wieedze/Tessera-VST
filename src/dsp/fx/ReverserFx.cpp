#include "ReverserFx.h"
#include <algorithm>

namespace tessera::dsp
{
    void ReverserFx::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        juce::ignoreUnused (maxBlockSize);
        reset();
    }

    void ReverserFx::reset()
    {
        anchored      = false;
        samplesPlayed = 0;
        playPos       = 0;
    }

    void ReverserFx::process (juce::AudioBuffer<float>& buffer,
                              const CaptureBuffer&     capture,
                              const FxParams&          params)
    {
        if (sampleRate <= 0.0)
            return; // not prepared

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = std::min (buffer.getNumChannels(), 2);

        // Hot loop. RT-safe: no alloc, no lock, no exception path.
        for (int i = 0; i < numSamples; ++i)
        {
            // Re-anchor when:
            //   - we have not anchored yet (first call after reset()), OR
            //   - we have played a full windowLength of samples — refresh the
            //     anchor to the most recent slice. This produces a series of
            //     "reversed micro-windows", each truly reversed, with the
            //     anchor following the live audio. Avoids the silent-anchor
            //     trap when the FX is activated between two sounds.
            if (! anchored || samplesPlayed >= anchoredWindowLength)
            {
                anchoredWindowLength = std::max (
                    1,
                    static_cast<int> (params.reverserWindowMs * 0.001 * sampleRate));
                startPos      = capture.getWritePos() - anchoredWindowLength;
                samplesPlayed = 0;
                playPos       = 0;
                anchored      = true;
            }

            // Symmetric reflection inside the current window: playPos 0..N-1
            // maps to (N-1)..0, so we read the captured slice in reverse.
            const double readPos = static_cast<double> (startPos + (anchoredWindowLength - 1 - playPos));

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, i, capture.readInterpolated (ch, readPos));

            ++playPos;
            ++samplesPlayed;
        }
    }

    FxType ReverserFx::getType() const
    {
        return FxType::Reverser;
    }
} // namespace tessera::dsp
