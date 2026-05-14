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

        // Detect if the input block is essentially silent (< -60 dB peak).
        // If so, we skip the cycle-end re-anchor: anchoring on silence would
        // make Stutter loop silence forever. Instead we keep the previous
        // anchor active — the "last good" loop continues until audio resumes.
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
            // duration_seconds = stutterRate * 240 / BPM  → samples = duration * sampleRate.
            if (! anchored)
            {
                // First anchor after reset() — always happens, even if the
                // input is silent: the user just triggered the FX, we owe
                // them an immediate response (silence if no audio is in
                // capture, which they will fix by playing something).
                anchoredLoopLength = std::max (
                    1,
                    static_cast<int> (params.stutterRate * 240.0 / params.bpm * sampleRate));
                startPos      = capture.getWritePos() - anchoredLoopLength;
                samplesPlayed = 0;
                playPos       = 0;
                anchored      = true;
            }
            else if (samplesPlayed >= anchoredLoopLength)
            {
                // Cycle end. Re-anchor on the latest slice ONLY if there is
                // audio coming in this block. If the input is silent, just
                // restart the same loop (keep startPos) so we don't replace
                // a "good" loop with one full of silence.
                if (! inputSilent)
                {
                    anchoredLoopLength = std::max (
                        1,
                        static_cast<int> (params.stutterRate * 240.0 / params.bpm * sampleRate));
                    startPos = capture.getWritePos() - anchoredLoopLength;
                }
                samplesPlayed = 0;
                playPos       = 0;
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
