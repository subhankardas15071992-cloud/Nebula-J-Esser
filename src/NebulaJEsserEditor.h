#pragma once

#include "NebulaJEsserProcessor.h"
#include "ui/Components.h"
#include "ui/SciFiLookAndFeel.h"

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

class NebulaJEsserAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit NebulaJEsserAudioProcessorEditor(NebulaJEsserAudioProcessor& processor);
    ~NebulaJEsserAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshPresetList();
    void showMidiMenu();
    void layoutTopBar(juce::Rectangle<int> area);
    void layoutCorePage();
    void layoutShapePage();
    void layoutRoutingPage();
    void layoutGrid(const std::vector<juce::Component*>& controls, juce::Rectangle<int> area, int preferredCellHeight);

    NebulaJEsserAudioProcessor& audioProcessor;
    nebula::ui::SciFiLookAndFeel lookAndFeel;

    juce::Label titleLabel;
    juce::Label companyLabel;
    juce::ComboBox presetBox;
    nebula::ui::IconButton presetSaveButton { "Save preset", nebula::ui::IconButton::Glyph::save };
    nebula::ui::IconButton undoButton { "Undo", nebula::ui::IconButton::Glyph::undo };
    nebula::ui::IconButton redoButton { "Redo", nebula::ui::IconButton::Glyph::redo };
    nebula::ui::IconButton abButton { "A/B", nebula::ui::IconButton::Glyph::ab };
    nebula::ui::IconButton midiButton { "MIDI learn", nebula::ui::IconButton::Glyph::midi };
    nebula::ui::IconButton bypassButton { "FX bypass", nebula::ui::IconButton::Glyph::bypass };

    nebula::ui::SpectrumPanel spectrum;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::Viewport coreViewport;
    juce::Viewport shapeViewport;
    juce::Viewport routingViewport;
    juce::Component corePage;
    juce::Component shapePage;
    juce::Component routingPage;

    nebula::ui::MeterStrip detectionMeter;
    nebula::ui::MeterStrip reductionMeter;
    nebula::ui::ParameterKnob tkeoKnob;
    nebula::ui::ParameterKnob annihilationKnob;
    nebula::ui::ParameterKnob mixKnob;
    nebula::ui::ParameterCombo modeCombo;
    nebula::ui::ParameterCombo profileCombo;
    nebula::ui::ParameterCombo rangeCombo;

    nebula::ui::ParameterKnob minFrequencyKnob;
    nebula::ui::ParameterKnob maxFrequencyKnob;
    nebula::ui::ParameterKnob cutWidthKnob;
    nebula::ui::ParameterKnob cutDepthKnob;
    nebula::ui::ParameterKnob cutSlopeKnob;
    nebula::ui::ParameterCombo filterCombo;
    nebula::ui::ParameterToggle filterSoloToggle;
    nebula::ui::ParameterToggle triggerHearToggle;

    nebula::ui::ParameterCombo sideChainCombo;
    nebula::ui::ParameterKnob stereoLinkKnob;
    nebula::ui::ParameterCombo stereoModeCombo;
    nebula::ui::ParameterToggle lookaheadToggle;
    nebula::ui::ParameterKnob lookaheadKnob;
    nebula::ui::ParameterKnob inputLevelKnob;
    nebula::ui::ParameterKnob inputPanKnob;
    nebula::ui::ParameterKnob outputLevelKnob;
    nebula::ui::ParameterKnob outputPanKnob;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    int knownPresetCount {};
};
