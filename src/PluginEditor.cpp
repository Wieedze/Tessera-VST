#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p)
    , processorRef (p)
    , genericEditor (p) // Build sliders/combos from p's parameter list (APVTS).
{
    addAndMakeVisible (genericEditor);
    addAndMakeVisible (inspectButton);

    // Melatonin inspector for live UI debugging (Pamplejuce default).
    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };

    // Wider/taller to fit the auto-generated parameter strip + inspector button.
    setSize (520, 360);
}

PluginEditor::~PluginEditor()
{
}

void PluginEditor::paint (juce::Graphics& g)
{
    // Background fill — the generic editor and the inspector button draw on top.
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();
    const int buttonHeight = 40;
    const int buttonMargin = 8;

    // Inspector button: small strip at the bottom.
    inspectButton.setBounds (area.removeFromBottom (buttonHeight).reduced (buttonMargin));

    // Generic editor: the rest of the window, auto-laying out one row per APVTS param.
    genericEditor.setBounds (area);
}
