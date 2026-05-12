#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Musical division values for the Stutter rate parameter, in fraction-of-whole-note units.
    // Indexed by the AudioParameterChoice index (0 = 1/2, 5 = 1/64).
    constexpr float kStutterRateValues[] = { 0.5f, 0.25f, 0.125f, 0.0625f, 0.03125f, 0.015625f };
    constexpr int   kStutterRateCount    = static_cast<int> (std::size (kStutterRateValues));
}

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
     , apvts (*this, nullptr, "Tessera", createParameterLayout())
{
    // Cache the atomic pointers ONCE. Reading via getRawParameterValue() in
    // processBlock would re-hash the string ID at audio rate (see lesson 0004).
    fxTypeParam      = apvts.getRawParameterValue ("fx_type");
    stutterRateParam = apvts.getRawParameterValue ("stutter_rate");
    jassert (fxTypeParam      != nullptr);
    jassert (stutterRateParam != nullptr);
}

PluginProcessor::~PluginProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // fx_type: which FX is active. Index in lock-step with tessera::dsp::FxType.
    layout.add (std::make_unique<juce::AudioParameterChoice> ( // RT-OK: parameter layout (host thread, init time)
        juce::ParameterID { "fx_type", 1 },
        "FX Type",
        juce::StringArray { "Thru", "Stutter" },
        0)); // default = Thru

    // stutter_rate: musical division for the Stutter loop window.
    layout.add (std::make_unique<juce::AudioParameterChoice> ( // RT-OK: parameter layout (host thread, init time)
        juce::ParameterID { "stutter_rate", 1 },
        "Stutter Rate",
        juce::StringArray { "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" },
        3)); // default = 1/16

    return layout;
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Pre-allocate the capture ring buffer + every FX's internal state.
    captureBuffer.prepare (sampleRate);
    fxBank.prepareAll      (sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer&         midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Guard against output-only channels containing stale memory.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // 1. Capture the incoming audio into the ring buffer BEFORE any processing,
    //    so the active FX can read the past via captureBuffer.readInterpolated().
    captureBuffer.write (buffer);

    // 2. Snapshot the relevant APVTS values for this block. Atomic loads —
    //    no string lookups in the audio path.
    const auto fxIdx   = static_cast<int> (fxTypeParam->load());
    const auto rateIdx = static_cast<int> (stutterRateParam->load());

    // 3. Build the FxParams snapshot (sampleRate, bpm, stutter rate, ...).
    tessera::dsp::FxParams params;
    params.sampleRate = getSampleRate();
    params.bpm        = 120.0;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpmOpt = pos->getBpm())
                params.bpm = *bpmOpt;
        }
    }
    params.stutterRate = kStutterRateValues[juce::jlimit (0, kStutterRateCount - 1, rateIdx)];

    // 4. Dispatch to the selected FX. O(1) lookup in FxBank (array index).
    const auto currentFx = static_cast<tessera::dsp::FxType> (
        juce::jlimit (0, static_cast<int> (tessera::dsp::FxType::Count) - 1, fxIdx));
    fxBank.get (currentFx).process (buffer, captureBuffer, params);
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
// State save / restore — runs on the host's message thread, NOT the audio
// path. Allocation is therefore fine here.

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) // RT-OK: getState runs on host message thread
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes)) // RT-OK: setState runs on host message thread
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
