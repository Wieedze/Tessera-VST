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
        anchored = false;
        playPos  = 0;
    }

    void ReverserFx::process (juce::AudioBuffer<float>& buffer,
                              const CaptureBuffer&     capture,
                              const FxParams&          params)
    {
        if (sampleRate <= 0.0)
            return; // not prepared

        // First call after reset(): anchor the window once and lock its length.
        // Window length in samples, derived from the user-set duration in ms.
        // Clamp to at least 1 to avoid divide-by-zero on the modulo below.
        if (! anchored)
        {
            anchoredWindowLength = std::max (
                1,
                static_cast<int> (params.reverserWindowMs * 0.001 * sampleRate));
            startPos = capture.getWritePos() - anchoredWindowLength;
            anchored = true;
        }

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = std::min (buffer.getNumChannels(), 2);

        // Hot loop. RT-safe: no alloc, no lock, no exception path.
        for (int i = 0; i < numSamples; ++i)
        {
            // The reflection: i goes 0 -> windowLength-1, but inside the window
            // we read (windowLength - 1 - relPos) which goes windowLength-1 -> 0.
            const int    relPos  = playPos % anchoredWindowLength;
            const double readPos = static_cast<double> (startPos + (anchoredWindowLength - 1 - relPos));

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, i, capture.readInterpolated (ch, readPos));

            ++playPos;
        }
    }

    FxType ReverserFx::getType() const
    {
        return FxType::Reverser;
    }
} // namespace tessera::dsp
