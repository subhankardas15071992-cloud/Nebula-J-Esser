#pragma once

#include "NebulaJEsserProcessor.h"
#include "ui/SciFiLookAndFeel.h"

#include <array>
#include <functional>
#include <memory>

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace nebula::ui
{
class NumericSlider final : public juce::Slider
{
public:
    std::function<void()> onRightClick;
    std::function<void()> onTouch;

    void mouseDown(const juce::MouseEvent& event) override;
};

class LearnComboBox final : public juce::ComboBox
{
public:
    std::function<void()> onTouch;
    void mouseDown(const juce::MouseEvent& event) override;
};

class LearnToggleButton final : public juce::ToggleButton
{
public:
    std::function<void()> onTouch;
    void mouseDown(const juce::MouseEvent& event) override;
};

class IconButton final : public juce::Button
{
public:
    enum class Glyph
    {
        undo,
        redo,
        save,
        ab,
        midi,
        bypass
    };

    explicit IconButton(const juce::String& name, Glyph glyphToUse);

    std::function<void()> onTouch;
    std::function<void()> onRightClick;
    void mouseDown(const juce::MouseEvent& event) override;
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;

private:
    Glyph glyph;
};

class ParameterKnob final : public juce::Component
{
public:
    ParameterKnob(NebulaJEsserAudioProcessor& processor,
                  juce::AudioProcessorValueTreeState& state,
                  const juce::String& parameterId,
                  const juce::String& labelText);

    void resized() override;

private:
    NebulaJEsserAudioProcessor& owner;
    juce::String id;
    NumericSlider knob;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class ParameterCombo final : public juce::Component
{
public:
    ParameterCombo(NebulaJEsserAudioProcessor& processor,
                   juce::AudioProcessorValueTreeState& state,
                   const juce::String& parameterId,
                   const juce::String& labelText);

    void resized() override;

private:
    NebulaJEsserAudioProcessor& owner;
    juce::String id;
    juce::Label label;
    LearnComboBox combo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

class ParameterToggle final : public juce::Component
{
public:
    ParameterToggle(NebulaJEsserAudioProcessor& processor,
                    juce::AudioProcessorValueTreeState& state,
                    const juce::String& parameterId,
                    const juce::String& text);

    void resized() override;

private:
    NebulaJEsserAudioProcessor& owner;
    juce::String id;
    LearnToggleButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

class MeterStrip final : public juce::Component
{
public:
    MeterStrip(NebulaJEsserAudioProcessor& processor,
               juce::AudioProcessorValueTreeState& state,
               const juce::String& parameterId,
               const juce::String& title,
               bool reductionMeter);

    void setMeterValues(float currentDb, float maxDb);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class ClickableLabel final : public juce::Label
    {
    public:
        std::function<void()> onClick;
        void mouseUp(const juce::MouseEvent& event) override;
    };

    NebulaJEsserAudioProcessor& owner;
    juce::String id;
    bool isReduction;
    float current {};
    float maximum {};
    juce::Label titleLabel;
    juce::Label currentField;
    ClickableLabel maxField;
    NumericSlider thresholdSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::Rectangle<int> meterBounds;
};

class SpectrumPanel final : public juce::Component
{
public:
    SpectrumPanel(NebulaJEsserAudioProcessor& processor, juce::AudioProcessorValueTreeState& state);

    void refresh();
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    enum class DragNode
    {
        none,
        minimum,
        maximum
    };

    [[nodiscard]] double frequencyForX(float x) const;
    [[nodiscard]] float xForFrequency(double frequency) const;
    [[nodiscard]] double parameterHz(const juce::String& id) const;
    void setParameterHz(const juce::String& id, double frequency);
    void drawNode(juce::Graphics& g, double frequency, const juce::Colour& colour, const juce::String& text, bool placeLabelNearTop);

    NebulaJEsserAudioProcessor& owner;
    juce::AudioProcessorValueTreeState& state;
    juce::dsp::FFT fft { 11 };
    juce::dsp::WindowingFunction<float> window { nebula::dsp::AnalyzerExchange::windowSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, nebula::dsp::AnalyzerExchange::windowSize> waveform {};
    std::array<float, nebula::dsp::AnalyzerExchange::windowSize * 2> fftData {};
    juce::Path spectrumPath;
    DragNode activeNode { DragNode::none };
};

class PresetNameBox final : public juce::Component
{
public:
    explicit PresetNameBox(std::function<void(juce::String)> saveCallback);
    void resized() override;

private:
    juce::TextEditor editor;
    juce::TextButton saveButton { "SAVE" };
    std::function<void(juce::String)> onSave;
};

void showNumericEntryBox(juce::Component& anchor, juce::RangedAudioParameter& parameter);
void showPresetNameBox(juce::Component& anchor, std::function<void(juce::String)> saveCallback);
} // namespace nebula::ui
