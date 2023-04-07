#pragma once

#include "RnNoiseAudioProcessor.h"

//==============================================================================
class RnNoiseAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit RnNoiseAudioProcessorEditor(RnNoiseAudioProcessor &p);

    ~RnNoiseAudioProcessorEditor() override;

    void resized() override;

    void paint(juce::Graphics &g) override;

private:
    juce::Label m_headerLabel;

    RnNoiseAudioProcessor &m_processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RnNoiseAudioProcessorEditor)
};