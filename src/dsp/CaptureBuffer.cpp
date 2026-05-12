#include "CaptureBuffer.h"
#include <cmath>

namespace tessera::dsp
{
    // -----------------------------------------------------------------
    // Stubs — implementations land in steps 3 to 5.
    // The signatures are final; the bodies are minimal so the linker is happy.
    // -----------------------------------------------------------------

    void CaptureBuffer::prepare (double newSampleRate)
    {
        constexpr int numChannels = 2; // stereo

        sampleRate = newSampleRate;
        bufferSize = static_cast<int> (newSampleRate * kMaxCaptureSeconds);

        // Allocate the ring buffer for worst-case duration at this sample rate.
        // setSize() may allocate — that's why this method is NOT RT-safe.
        ringBuffer.setSize (numChannels,
                            bufferSize,
                            /* keepExistingContent */ false,
                            /* clearExtraSpace     */ true,
                            /* avoidReallocating   */ false);

        ringBuffer.clear(); // belt-and-suspenders: zero everything explicitly.

        // Reset state in case prepare() is called twice (host sample-rate change).
        writePos.store (0);
        frozen.store   (false);
    }

    void CaptureBuffer::write (const juce::AudioBuffer<float>& input) noexcept
    {
        if (frozen.load())   return;   // freeze => no write
        if (bufferSize == 0) return;   // prepare() never called: skip safely

        const int     numSamples       = input.getNumSamples();
        const int     numChannelsToCopy = juce::jmin (input.getNumChannels(),
                                                       ringBuffer.getNumChannels());
        const int64_t startPos          = writePos.load();

        // Hot loop. No allocation, no lock, no exception path.
        for (int ch = 0; ch < numChannelsToCopy; ++ch)
        {
            const float* src = input.getReadPointer (ch);
            float*       dst = ringBuffer.getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                const int idx = static_cast<int> ((startPos + i) % bufferSize);
                dst[idx] = src[i];
            }
        }

        // Publish the new write position. Atomic store ensures any
        // concurrent reader sees either the pre-write or post-write state,
        // never a torn intermediate.
        writePos.store (startPos + numSamples);
    }

    float CaptureBuffer::readInterpolated (int channel, double samplePos) const noexcept
    {
        // Defensive guards — return silence rather than crash on misuse.
        if (bufferSize == 0)                                  return 0.0f;
        if (channel < 0 || channel >= ringBuffer.getNumChannels())
            return 0.0f;

        // Split samplePos into integer + fractional parts.
        // std::floor (not static_cast<int>) so negative positions round the right way.
        const double  floorPos = std::floor (samplePos);
        const float   frac     = static_cast<float> (samplePos - floorPos);
        const int64_t i0       = static_cast<int64_t> (floorPos);
        const int64_t i1       = i0 + 1;

        // Wrap into [0, bufferSize). The ((x % N) + N) % N trick handles negatives.
        const int idx0 = static_cast<int> (((i0 % bufferSize) + bufferSize) % bufferSize);
        const int idx1 = static_cast<int> (((i1 % bufferSize) + bufferSize) % bufferSize);

        const float* data = ringBuffer.getReadPointer (channel);
        const float  s0   = data[idx0];
        const float  s1   = data[idx1];

        // Linear interpolation: barycentric blend of s0 and s1, weighted by frac.
        return s0 + frac * (s1 - s0);
    }

    void CaptureBuffer::freeze (bool shouldFreeze) noexcept
    {
        frozen.store (shouldFreeze);
    }

    bool CaptureBuffer::isFrozen() const noexcept
    {
        return frozen.load();
    }

    int64_t CaptureBuffer::getWritePos() const noexcept
    {
        return writePos.load();
    }

    int CaptureBuffer::getBufferSize() const noexcept
    {
        return bufferSize;
    }
} // namespace tessera::dsp
