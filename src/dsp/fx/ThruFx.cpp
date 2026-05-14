#include "ThruFx.h"

namespace tessera::dsp
{
    void ThruFx::prepare (double sampleRate, int maxBlockSize)
    {
        // Nothing to allocate — ThruFx is stateless.
        juce::ignoreUnused (sampleRate, maxBlockSize);
    }

    void ThruFx::reset()
    {
        // No internal state to reset.
    }

    void ThruFx::process (juce::AudioBuffer<float>& buffer,
                          const CaptureBuffer&     capture,
                          const FxParams&          params)
    {
        // Bypass: the input is already in `buffer`. We do nothing — it passes through.
        juce::ignoreUnused (buffer, capture, params);
    }

    FxType ThruFx::getType() const
    {
        return FxType::Thru;
    }
} // namespace tessera::dsp
