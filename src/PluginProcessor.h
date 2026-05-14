#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/CaptureBuffer.h"
#include "dsp/fx/FxBank.h"

#if (MSVC)
#include "ipps.h"
#endif

class PluginProcessor : public juce::AudioProcessor
{
public:
    // Re-introduce the AudioBuffer<double> overload from the base class so it
    // is not silently hidden by the AudioBuffer<float> override below.
    using juce::AudioProcessor::processBlock;

    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /// Exposed for the editor and for tests.
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    tessera::dsp::CaptureBuffer captureBuffer;
    tessera::dsp::FxBank        fxBank;

    // Cached APVTS pointers — refreshed in the constructor (after apvts is built)
    // and read via std::atomic<float>::load() in processBlock. NEVER call
    // apvts.getRawParameterValue() inside the audio path (string hash cost).
    std::atomic<float>* fxTypeParam          { nullptr };
    std::atomic<float>* stutterRateParam     { nullptr };
    std::atomic<float>* reverserWindowParam  { nullptr };
    std::atomic<float>* tapeStopLengthParam  { nullptr };
    std::atomic<float>* tapeStopCurveParam   { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
