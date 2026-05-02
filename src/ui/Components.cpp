#include "ui/Components.h"

#include "state/Parameters.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace nebula::ui
{
namespace
{
class NumericEntryBox final : public juce::Component
{
public:
    explicit NumericEntryBox(juce::RangedAudioParameter& parameterToEdit)
        : parameter(parameterToEdit)
    {
        addAndMakeVisible(editor);
        addAndMakeVisible(apply);
        editor.setSelectAllWhenFocused(true);
        editor.setText(parameter.getText(parameter.getValue(), 64), false);
        editor.onReturnKey = [this] { commit(); };
        editor.onEscapeKey = [this] { dismiss(); };
        apply.onClick = [this] { commit(); };
        setSize(180, 54);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        apply.setBounds(area.removeFromRight(54));
        area.removeFromRight(6);
        editor.setBounds(area);
    }

private:
    void commit()
    {
        const auto normalized = nebula::state::normalizedValueForText(parameter, editor.getText());
        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost(normalized);
        parameter.endChangeGesture();
        dismiss();
    }

    void dismiss()
    {
        if (auto* callout = findParentComponentOfClass<juce::CallOutBox>())
            callout->dismiss();
    }

    juce::RangedAudioParameter& parameter;
    juce::TextEditor editor;
    juce::TextButton apply { "SET" };
};

[[nodiscard]] juce::RangedAudioParameter* findParameter(juce::AudioProcessorValueTreeState& state, const juce::String& id)
{
    return state.getParameter(id);
}

[[nodiscard]] float normalizedFrequency(double frequency)
{
    const auto minFrequency = std::log(20.0);
    const auto maxFrequency = std::log(20000.0);
    return static_cast<float>((std::log(std::clamp(frequency, 20.0, 20000.0)) - minFrequency) / (maxFrequency - minFrequency));
}
} // namespace

void NumericSlider::mouseDown(const juce::MouseEvent& event)
{
    if (onTouch)
        onTouch();

    if (event.mods.isPopupMenu())
    {
        if (onRightClick)
            onRightClick();
        return;
    }

    juce::Slider::mouseDown(event);
}

void LearnComboBox::mouseDown(const juce::MouseEvent& event)
{
    if (onTouch)
        onTouch();
    juce::ComboBox::mouseDown(event);
}

void LearnToggleButton::mouseDown(const juce::MouseEvent& event)
{
    if (onTouch)
        onTouch();
    juce::ToggleButton::mouseDown(event);
}

IconButton::IconButton(const juce::String& name, Glyph glyphToUse)
    : juce::Button(name),
      glyph(glyphToUse)
{
    setClickingTogglesState(false);
    setTooltip(name);
}

void IconButton::mouseDown(const juce::MouseEvent& event)
{
    if (onTouch)
        onTouch();

    if (event.mods.isPopupMenu())
    {
        if (onRightClick)
            onRightClick();
        return;
    }
    juce::Button::mouseDown(event);
}

void IconButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    SciFiLookAndFeel look;
    look.drawButtonBackground(g, *this, SciFiLookAndFeel::panelRaised(), highlighted, down);

    const auto bounds = getLocalBounds().toFloat().reduced(8.0f);
    g.setColour(getToggleState() ? SciFiLookAndFeel::voidBlack() : SciFiLookAndFeel::text());
    g.setFont(juce::Font { juce::FontOptions(std::max(11.0f, bounds.getHeight() * 0.42f), juce::Font::bold) });

    juce::Path path;
    const auto c = bounds.getCentre();
    const auto r = std::min(bounds.getWidth(), bounds.getHeight()) * 0.34f;

    switch (glyph)
    {
        case Glyph::undo:
            path.addArc(c.x - r, c.y - r, r * 2.0f, r * 2.0f, 0.2f, 4.9f, true);
            path.lineTo(c.x - r * 1.08f, c.y - r * 0.42f);
            break;
        case Glyph::redo:
            path.addArc(c.x - r, c.y - r, r * 2.0f, r * 2.0f, -1.8f, 2.8f, true);
            path.lineTo(c.x + r * 1.08f, c.y - r * 0.42f);
            break;
        case Glyph::save:
            path.addRoundedRectangle(bounds.reduced(3.0f), 2.0f);
            path.addRectangle(bounds.getX() + bounds.getWidth() * 0.25f, bounds.getY() + 3.0f, bounds.getWidth() * 0.5f, bounds.getHeight() * 0.28f);
            break;
        case Glyph::ab:
            g.drawFittedText("A/B", getLocalBounds().reduced(4), juce::Justification::centred, 1);
            return;
        case Glyph::midi:
            g.drawFittedText("MIDI", getLocalBounds().reduced(3), juce::Justification::centred, 1);
            return;
        case Glyph::bypass:
            path.addEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
            path.startNewSubPath(c.x, c.y - r * 1.25f);
            path.lineTo(c.x, c.y);
            break;
    }

    g.strokePath(path, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

ParameterKnob::ParameterKnob(NebulaJEsserAudioProcessor& processor,
                             juce::AudioProcessorValueTreeState& state,
                             const juce::String& parameterId,
                             const juce::String& labelText)
    : owner(processor),
      id(parameterId)
{
    addAndMakeVisible(knob);
    addAndMakeVisible(label);
    knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 20);
    knob.setTooltip(labelText);
    knob.setDoubleClickReturnValue(false, 0.0);
    knob.onTouch = [this] { owner.selectMidiLearnTarget(id); };
    if (auto* parameter = findParameter(state, id))
        knob.onRightClick = [this, parameter] { showNumericEntryBox(knob, *parameter); };
    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, SciFiLookAndFeel::mutedText());
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, id, knob);
}

void ParameterKnob::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromBottom(18));
    knob.setBounds(area.reduced(1));
}

ParameterCombo::ParameterCombo(NebulaJEsserAudioProcessor& processor,
                               juce::AudioProcessorValueTreeState& stateToUse,
                               const juce::String& parameterId,
                               const juce::String& labelText)
    : owner(processor),
      id(parameterId)
{
    addAndMakeVisible(label);
    addAndMakeVisible(combo);
    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, SciFiLookAndFeel::mutedText());
    combo.onTouch = [this] { owner.selectMidiLearnTarget(id); };

    if (const auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(stateToUse.getParameter(id)); parameter != nullptr)
        combo.addItemList(parameter->choices, 1);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(stateToUse, id, combo);
}

void ParameterCombo::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromTop(18));
    combo.setBounds(area.reduced(0, 2));
}

ParameterToggle::ParameterToggle(NebulaJEsserAudioProcessor& processor,
                                 juce::AudioProcessorValueTreeState& state,
                                 const juce::String& parameterId,
                                 const juce::String& text)
    : owner(processor),
      id(parameterId)
{
    addAndMakeVisible(button);
    button.setButtonText(text);
    button.onTouch = [this] { owner.selectMidiLearnTarget(id); };
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, id, button);
}

void ParameterToggle::resized()
{
    button.setBounds(getLocalBounds().reduced(1));
}

MeterStrip::MeterStrip(NebulaJEsserAudioProcessor& processor,
                       juce::AudioProcessorValueTreeState& state,
                       const juce::String& parameterId,
                       const juce::String& title,
                       bool reductionMeter)
    : owner(processor),
      id(parameterId),
      isReduction(reductionMeter)
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(currentField);
    addAndMakeVisible(maxField);
    addAndMakeVisible(thresholdSlider);

    titleLabel.setText(title, juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, SciFiLookAndFeel::text());
    currentField.setJustificationType(juce::Justification::centred);
    currentField.setColour(juce::Label::backgroundColourId, SciFiLookAndFeel::panelRaised());
    maxField.setJustificationType(juce::Justification::centred);
    maxField.setColour(juce::Label::backgroundColourId, SciFiLookAndFeel::panelRaised());
    maxField.setTooltip("Click to reset peak hold");
    maxField.onClick = [this]
    {
        if (isReduction)
            owner.resetReductionMax();
        else
            owner.resetDetectionMax();
    };

    thresholdSlider.setSliderStyle(juce::Slider::LinearVertical);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    thresholdSlider.onTouch = [this] { owner.selectMidiLearnTarget(id); };
    if (auto* parameter = findParameter(state, id))
        thresholdSlider.onRightClick = [this, parameter] { showNumericEntryBox(thresholdSlider, *parameter); };
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, id, thresholdSlider);
}

void MeterStrip::ClickableLabel::mouseUp(const juce::MouseEvent& event)
{
    if (! event.mods.isPopupMenu() && onClick)
        onClick();
    juce::Label::mouseUp(event);
}

void MeterStrip::setMeterValues(float currentDb, float maxDb)
{
    current = currentDb;
    maximum = maxDb;
    const auto suffix = isReduction ? " dB" : " dBFS";
    currentField.setText(juce::String(current, 1) + suffix, juce::dontSendNotification);
    maxField.setText(juce::String(maximum, 1) + suffix, juce::dontSendNotification);
    repaint();
}

void MeterStrip::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(SciFiLookAndFeel::panel().withAlpha(0.92f));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(SciFiLookAndFeel::grid());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    const auto meter = meterBounds.toFloat();
    g.setColour(SciFiLookAndFeel::voidBlack());
    g.fillRoundedRectangle(meter, 4.0f);
    g.setColour(SciFiLookAndFeel::grid().withAlpha(0.75f));
    for (int i = 1; i < 5; ++i)
    {
        const auto y = meter.getY() + meter.getHeight() * static_cast<float>(i) / 5.0f;
        g.drawHorizontalLine(static_cast<int>(std::round(y)), meter.getX(), meter.getRight());
    }

    const auto normalized = isReduction
        ? std::clamp(current / 40.0f, 0.0f, 1.0f)
        : std::clamp((current + 80.0f) / 80.0f, 0.0f, 1.0f);
    auto fill = meter.withY(meter.getBottom() - meter.getHeight() * normalized);
    fill.setBottom(meter.getBottom());
    juce::ColourGradient gradient(SciFiLookAndFeel::cyan(), fill.getX(), fill.getBottom(), SciFiLookAndFeel::magenta(), fill.getRight(), fill.getY(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(fill, 4.0f);
}

void MeterStrip::resized()
{
    auto area = getLocalBounds().reduced(8);
    titleLabel.setBounds(area.removeFromTop(18));
    auto fields = area.removeFromBottom(22);
    currentField.setBounds(fields.removeFromLeft(fields.getWidth() / 2).reduced(2));
    maxField.setBounds(fields.reduced(2));
    area.removeFromBottom(4);
    thresholdSlider.setBounds(area.removeFromRight(76));
    area.removeFromRight(6);
    meterBounds = area;
}

SpectrumPanel::SpectrumPanel(NebulaJEsserAudioProcessor& processor, juce::AudioProcessorValueTreeState& stateToUse)
    : owner(processor),
      state(stateToUse)
{
    setInterceptsMouseClicks(true, true);
}

void SpectrumPanel::refresh()
{
    owner.analyzerExchange().copyWindow(waveform);
    fftData.fill(0.0f);
    std::copy(waveform.begin(), waveform.end(), fftData.begin());
    window.multiplyWithWindowingTable(fftData.data(), nebula::dsp::AnalyzerExchange::windowSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data(), true);
    repaint();
}

double SpectrumPanel::frequencyForX(float x) const
{
    const auto bounds = getLocalBounds().reduced(14, 12).toFloat();
    const auto normalized = std::clamp((x - bounds.getX()) / std::max(bounds.getWidth(), 1.0f), 0.0f, 1.0f);
    return std::exp(std::log(20.0) + normalized * (std::log(20000.0) - std::log(20.0)));
}

float SpectrumPanel::xForFrequency(double frequency) const
{
    const auto bounds = getLocalBounds().reduced(14, 12).toFloat();
    return bounds.getX() + bounds.getWidth() * normalizedFrequency(frequency);
}

double SpectrumPanel::parameterHz(const juce::String& id) const
{
    if (const auto* value = state.getRawParameterValue(id); value != nullptr)
        return static_cast<double>(value->load());
    return 1000.0;
}

void SpectrumPanel::setParameterHz(const juce::String& id, double frequency)
{
    if (auto* parameter = state.getParameter(id))
    {
        const auto normalized = parameter->convertTo0to1(static_cast<float>(std::clamp(frequency, 20.0, 20000.0)));
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(normalized);
        parameter->endChangeGesture();
    }
}

void SpectrumPanel::drawNode(juce::Graphics& g, double frequency, const juce::Colour& colour, const juce::String& text, bool placeLabelNearTop)
{
    const auto bounds = getLocalBounds().reduced(14, 12).toFloat();
    const auto x = xForFrequency(frequency);
    g.setColour(colour.withAlpha(0.8f));
    g.drawLine(x, bounds.getY(), x, bounds.getBottom(), 1.2f);
    g.setColour(colour);
    g.fillEllipse(x - 6.0f, bounds.getCentreY() - 6.0f, 12.0f, 12.0f);
    g.setColour(SciFiLookAndFeel::text());
    g.setFont(juce::Font { juce::FontOptions(11.0f) });
    const auto labelY = placeLabelNearTop ? static_cast<int>(bounds.getY() + 4.0f) : static_cast<int>(bounds.getBottom() - 18.0f);
    g.drawFittedText(text, juce::Rectangle<int>(static_cast<int>(x - 38.0f), labelY, 76, 16), juce::Justification::centred, 1);
}

void SpectrumPanel::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(SciFiLookAndFeel::panel());
    g.fillRoundedRectangle(bounds, 7.0f);
    g.setColour(SciFiLookAndFeel::grid());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 7.0f, 1.0f);

    const auto plot = getLocalBounds().reduced(14, 12).toFloat();
    g.setColour(SciFiLookAndFeel::grid().withAlpha(0.42f));
    for (int line = 1; line < 6; ++line)
    {
        const auto y = plot.getY() + plot.getHeight() * static_cast<float>(line) / 6.0f;
        g.drawHorizontalLine(static_cast<int>(std::round(y)), plot.getX(), plot.getRight());
    }

    spectrumPath.clear();
    const auto bins = nebula::dsp::AnalyzerExchange::windowSize / 2;
    for (int bin = 1; bin < bins; ++bin)
    {
        const auto sampleRate = std::max(1000.0, owner.getSampleRate());
        const auto frequency = static_cast<double>(bin) * sampleRate / static_cast<double>(nebula::dsp::AnalyzerExchange::windowSize);
        const auto x = xForFrequency(std::clamp(frequency, 20.0, 20000.0));
        const auto db = juce::Decibels::gainToDecibels(std::max(fftData[static_cast<std::size_t>(bin)], 1.0e-5f), -100.0f);
        const auto normalized = juce::jlimit(0.0f, 1.0f, (db + 90.0f) / 90.0f);
        const auto y = plot.getBottom() - plot.getHeight() * normalized;
        if (bin == 1)
            spectrumPath.startNewSubPath(x, y);
        else
            spectrumPath.lineTo(x, y);
    }

    juce::ColourGradient gradient(SciFiLookAndFeel::cyan(), plot.getX(), plot.getCentreY(), SciFiLookAndFeel::magenta(), plot.getRight(), plot.getCentreY(), false);
    g.setGradientFill(gradient);
    g.strokePath(spectrumPath, juce::PathStrokeType(1.8f));

    const auto minFrequency = parameterHz(nebula::state::ids::minFrequency.data());
    const auto maxFrequency = parameterHz(nebula::state::ids::maxFrequency.data());
    const auto nodesAreTight = std::abs(xForFrequency(minFrequency) - xForFrequency(maxFrequency)) < 84.0f;
    drawNode(g, minFrequency, SciFiLookAndFeel::cyan(), "MIN", nodesAreTight);
    drawNode(g, maxFrequency, SciFiLookAndFeel::magenta(), "MAX", false);
}

void SpectrumPanel::mouseDown(const juce::MouseEvent& event)
{
    const auto minX = xForFrequency(parameterHz(nebula::state::ids::minFrequency.data()));
    const auto maxX = xForFrequency(parameterHz(nebula::state::ids::maxFrequency.data()));
    const auto x = static_cast<float>(event.x);
    activeNode = std::abs(x - minX) < std::abs(x - maxX) ? DragNode::minimum : DragNode::maximum;
    owner.selectMidiLearnTarget(activeNode == DragNode::minimum
        ? nebula::state::ids::minFrequency.data()
        : nebula::state::ids::maxFrequency.data());
}

void SpectrumPanel::mouseDrag(const juce::MouseEvent& event)
{
    if (activeNode == DragNode::none)
        return;

    auto frequency = frequencyForX(static_cast<float>(event.x));
    const auto minFrequency = parameterHz(nebula::state::ids::minFrequency.data());
    const auto maxFrequency = parameterHz(nebula::state::ids::maxFrequency.data());
    if (activeNode == DragNode::minimum)
    {
        frequency = std::min(frequency, maxFrequency - 20.0);
        setParameterHz(nebula::state::ids::minFrequency.data(), frequency);
    }
    else
    {
        frequency = std::max(frequency, minFrequency + 20.0);
        setParameterHz(nebula::state::ids::maxFrequency.data(), frequency);
    }
}

void SpectrumPanel::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    activeNode = DragNode::none;
}

PresetNameBox::PresetNameBox(std::function<void(juce::String)> saveCallback)
    : onSave(std::move(saveCallback))
{
    addAndMakeVisible(editor);
    addAndMakeVisible(saveButton);
    editor.setText("Nebula Preset", false);
    editor.setSelectAllWhenFocused(true);
    editor.onReturnKey = [this]
    {
        if (onSave)
            onSave(editor.getText());
        if (auto* callout = findParentComponentOfClass<juce::CallOutBox>())
            callout->dismiss();
    };
    saveButton.onClick = editor.onReturnKey;
    setSize(240, 54);
}

void PresetNameBox::resized()
{
    auto area = getLocalBounds().reduced(8);
    saveButton.setBounds(area.removeFromRight(68));
    area.removeFromRight(6);
    editor.setBounds(area);
}

void showNumericEntryBox(juce::Component& anchor, juce::RangedAudioParameter& parameter)
{
    auto content = std::make_unique<NumericEntryBox>(parameter);
    juce::CallOutBox::launchAsynchronously(std::move(content), anchor.getScreenBounds(), anchor.getTopLevelComponent());
}

void showPresetNameBox(juce::Component& anchor, std::function<void(juce::String)> saveCallback)
{
    auto content = std::make_unique<PresetNameBox>(std::move(saveCallback));
    juce::CallOutBox::launchAsynchronously(std::move(content), anchor.getScreenBounds(), anchor.getTopLevelComponent());
}
} // namespace nebula::ui
