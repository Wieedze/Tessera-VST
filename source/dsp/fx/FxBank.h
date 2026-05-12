#pragma once

#include <array>
#include <memory>
#include "IFxModule.h"

namespace tessera::dsp
{
    /**
     * @file FxBank.h
     * @brief Registry that owns every FX instance for the plugin's lifetime.
     *
     * Pre-allocates all concrete FX at construction time so the audio path
     * never needs to allocate. The Sequencer (later) calls get(type) to
     * obtain a reference to the desired FX module and invokes its process().
     *
     * Ownership model: each slot owns its FX via std::unique_ptr<IFxModule>.
     * The destructor of FxBank cascades the destruction of every FX via the
     * virtual destructor declared in IFxModule.
     *
     * @see docs/architecture-engines.md §3
     */
    class FxBank
    {
    public:
        FxBank();
        ~FxBank() = default;

        /// Calls prepare() on every owned FX. Allocation allowed (NOT RT-safe).
        /// @note Call once from PluginProcessor::prepareToPlay.
        void prepareAll (double sampleRate, int maxBlockSize);

        /// Calls reset() on every owned FX. RT-safe (no allocation).
        void resetAll();

        /// Returns a reference to the FX of the requested type.
        /// @note RT-safe. Caller MUST pass a valid FxType (not Count, in-range).
        IFxModule& get (FxType type);

    private:
        std::array<std::unique_ptr<IFxModule>, kNumFxTypes> modules;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxBank)
    };
} // namespace tessera::dsp
