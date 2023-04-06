#include "RnNoiseAudioProcessor.h"
#include "RnNoisePluginEditor.h"
#include "common/RnNoiseCommonPlugin.h"

#include <memory>

//==============================================================================
RnNoiseAudioProcessorEditor::RnNoiseAudioProcessorEditor(RnNoiseAudioProcessor &p)
        : AudioProcessorEditor(&p), m_processorRef(p) {
    juce::ignoreUnused(m_processorRef);

    addAndMakeVisible(m_headerLabel);
    m_headerLabel.setText("Noise Suppressor for Voice", juce::dontSendNotification);
    m_headerLabel.setFont(juce::Font(26.0f, juce::Font::bold | juce::Font::underlined));
    m_headerLabel.setJustificationType(juce::Justification::centred);

    setSize(400, 100);
}

void RnNoiseAudioProcessorEditor::paint(juce::Graphics &g) {
    g.fillAll(juce::Colours::black);
}

void RnNoiseAudioProcessorEditor::resized() {
    juce::FlexBox flexBox;
    flexBox.flexWrap = juce::FlexBox::Wrap::wrap;
    flexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    flexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
    flexBox.flexDirection = juce::FlexBox::Direction::column;

    float width = static_cast<float>(getLocalBounds().getWidth());

    flexBox.items.add(juce::FlexItem(m_headerLabel).withWidth(width).withFlex(1.0));
    
    flexBox.performLayout(getLocalBounds().toFloat());
}

RnNoiseAudioProcessorEditor::~RnNoiseAudioProcessorEditor() = default;