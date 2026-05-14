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

        // Detect if the input block is essentially silent (< -60 dB peak).
        // If so, we skip the cycle-end re-anchor: anchoring on silence would
        // make Reverser replay silence forever. Instead we keep the previous
        // anchor active — the "last good" reversed window keeps looping until
        // audio resumes. Musically this gives a "freeze on release" feel
        // rather than a hard audio cut.
        constexpr float kSilenceThreshold = 1.0e-3f; // ~-60 dBFS
        bool inputSilent = true;
        for (int ch = 0; ch < numChannels && inputSilent; ++ch)
        {
            const float* in = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                if (std::abs (in[i]) > kSilenceThreshold)
                {
                    inputSilent = false;
                    break;
                }
            }
        }

        // Hot loop. RT-safe: no alloc, no lock, no exception path.
        for (int i = 0; i < numSamples; ++i)
        {
            if (! anchored)
            {
                // First anchor after reset() — always happens, even if the
                // input is silent: the user just triggered the FX, we owe
                // them an immediate response.
                anchoredWindowLength = std::max (
                    1,
                    static_cast<int> (params.reverserWindowMs * 0.001 * sampleRate));
                startPos      = capture.getWritePos() - anchoredWindowLength;
                samplesPlayed = 0;
                playPos       = 0;
                anchored      = true;
            }
            else if (samplesPlayed >= anchoredWindowLength)
            {
                // Cycle end. Re-anchor on the latest slice ONLY if there is
                // audio coming in this block. If the input is silent, just
                // restart the same reversed window (keep startPos) so we
                // don't replace a "good" window with one full of silence.
                if (! inputSilent)
                {
                    anchoredWindowLength = std::max (
                        1,
                        static_cast<int> (params.reverserWindowMs * 0.001 * sampleRate));
                    startPos = capture.getWritePos() - anchoredWindowLength;
                }
                samplesPlayed = 0;
                playPos       = 0;
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
