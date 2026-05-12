#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../CaptureBuffer.h"

namespace tessera::dsp
{
    /**
     * @file IFxModule.h
     * @brief Pure virtual interface for all FX modules in Tessera.
     *
     * Implements the Strategy Pattern: a single contract (this interface)
     * with multiple concrete strategies (StutterFx, ReverserFx, FilterFx...).
     * The Sequencer picks which strategy to invoke per step at runtime.
     *
     * Every FX is pre-allocated at startup inside FxBank. Allocation in
     * the audio path is forbidden — see .claude/rules/rt-safety.md.
     *
     * @see docs/architecture-engines.md §3
     */

    /// All concrete FX types known to the system. Used by the Sequencer
    /// to address a specific FX, and by FxBank as a lookup key.
    enum class FxType
    {
        Thru,    // bypass — input passes through unchanged
        Stutter, // re-read a recent slice of capture buffer at a musical rate
        // Reverser, TapeStop, Filter, Bitcrusher, Gater, PitchShifter, Granular, Slicer
        // will be added in upcoming weeks.
    };

    /**
     * @brief Plain-data parameters passed to every FX::process() call.
     *
     * Snapshot of the relevant FX state for one audio block. No allocation,
     * no virtual indirection — just values. Concrete FX read what they need
     * and ignore the rest.
     *
     * As more FX are added, this will grow. We keep one shared struct for
     * simplicity until the union of fields hurts readability.
     */
    struct FxParams
    {
        // Host/transport context (common to all FX)
        double sampleRate { 48000.0 };
        double bpm        { 120.0 };

        // Stutter-specific
        float stutterRate  { 0.0625f }; // 1/16 note by default
        float stutterDecay { 0.0f };    // 0 = no decay across repeats
        float stutterGate  { 1.0f };    // [0..1] fraction of the slice that plays
    };

    /**
     * @brief Pure virtual base class for FX modules.
     *
     * Subclasses must override the 4 pure virtual methods.
     */
    class IFxModule
    {
    public:
        // VIRTUAL DESTRUCTOR — required because we delete through IFxModule*
        // (FxBank owns each FX via std::unique_ptr<IFxModule>). Without virtual
        // here, derived destructors would NOT run when the unique_ptr resets.
        virtual ~IFxModule() = default;

        /// One-time setup at host start (after construction, before processing).
        /// Allowed to allocate. NOT RT-safe.
        virtual void prepare (double sampleRate, int maxBlockSize) = 0;

        /// Reset internal state without reallocating. Called when transport
        /// resets or pattern changes. RT-safe.
        virtual void reset() = 0;

        /// Process one audio block (in-place).
        /// @param buffer  The current block (read input, write output here).
        /// @param capture Shared read-only access to the recent past of the input.
        /// @param params  Parameter snapshot for this block.
        /// @note RT-safe. No allocation, no lock, no exception allowed.
        virtual void process (juce::AudioBuffer<float>& buffer,
                              const CaptureBuffer&     capture,
                              const FxParams&          params) = 0;

        /// Which FxType this is. Used by FxBank to register/lookup the module.
        /// @note RT-safe (typically returns a hard-coded constant).
        virtual FxType getType() const = 0;
    };
} // namespace tessera::dsp
