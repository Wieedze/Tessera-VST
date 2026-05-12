#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cstdint>

namespace tessera::dsp
{
    /**
     * @file CaptureBuffer.h
     * @brief Lock-free ring buffer for real-time audio capture.
     *
     * Stores the recent past of the input signal so DSP modules
     * (Stutter, Reverser, Granular, Slicer) can read back arbitrary
     * positions, including fractional ones (for pitch-shift / granular).
     *
     * Allocation happens in prepare(). Read/write in the audio path is
     * lock-free via std::atomic<int64_t> writePos.
     *
     * @see docs/architecture-engines.md §1
     */
    class CaptureBuffer
    {
    public:
        /// Maximum capture duration in seconds. 4 bars at 30 BPM = 32 s.
        /// Sized for worst case so any tempo from 30 BPM upwards fits.
        static constexpr double kMaxCaptureSeconds = 32.0;

        CaptureBuffer()  = default;
        ~CaptureBuffer() = default;

        /**
         * @brief Pre-allocate the ring buffer for kMaxCaptureSeconds at sampleRate, stereo.
         * @param sampleRate Host sample rate (Hz).
         * @note Called from prepareToPlay — may allocate. NOT RT-safe.
         */
        void prepare (double sampleRate);

        /**
         * @brief Write one audio block into the ring buffer.
         * @param input The block to capture (typically processBlock's input).
         * @note RT-safe. No-op if frozen.
         */
        void write (const juce::AudioBuffer<float>& input) noexcept;

        /**
         * @brief Read a single sample at a fractional position with linear interpolation.
         * @param channel 0 = left, 1 = right.
         * @param samplePos Absolute sample position (double, may be fractional).
         * @return Interpolated sample value.
         * @note RT-safe.
         */
        float readInterpolated (int channel, double samplePos) const noexcept;

        /**
         * @brief Freeze the buffer (write becomes a no-op).
         * @note Thread-safe via atomic.
         */
        void freeze (bool shouldFreeze) noexcept;

        /**
         * @brief Whether the buffer is currently frozen.
         */
        bool isFrozen() const noexcept;

        /**
         * @brief Current write position (global, sample-accurate, never wraps).
         * @return Monotonic counter. Read-back positions are derived from this.
         */
        int64_t getWritePos() const noexcept;

        /**
         * @brief Allocated buffer size in samples per channel.
         */
        int getBufferSize() const noexcept;

    private:
        juce::AudioBuffer<float> ringBuffer;
        std::atomic<int64_t>     writePos   { 0 };
        std::atomic<bool>        frozen     { false };
        int                      bufferSize { 0 };
        double                   sampleRate { 0.0 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CaptureBuffer)
    };
} // namespace tessera::dsp
