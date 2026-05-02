#include "state/Parameters.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace nebula::state
{
namespace
{
using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

[[nodiscard]] double parseNumber(juce::String text)
{
    text = text.trim().toLowerCase();
    const auto hasKiloSuffix = text.containsChar('k');
    const auto numeric = text.retainCharacters("0123456789.-");
    auto value = static_cast<double>(numeric.getDoubleValue());
    if (hasKiloSuffix)
        value *= 1000.0;
    return value;
}

[[nodiscard]] juce::String formatFixed(float value, int decimals)
{
    return juce::String(value, decimals);
}

[[nodiscard]] juce::String stringId(std::string_view id)
{
    return juce::String::fromUTF8(id.data(), static_cast<int>(id.size()));
}

[[nodiscard]] std::unique_ptr<juce::AudioParameterFloat> makeFloat(std::string_view id,
                                                                   const juce::String& name,
                                                                   juce::NormalisableRange<float> range,
                                                                   float defaultValue,
                                                                   const juce::String& unit,
                                                                   juce::AudioParameterFloatAttributes attributes = {})
{
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { stringId(id), 1 },
        name,
        range,
        defaultValue,
        attributes.withLabel(unit));
}

[[nodiscard]] std::unique_ptr<juce::AudioParameterBool> makeBool(std::string_view id,
                                                                 const juce::String& name,
                                                                 bool defaultValue)
{
    return std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { stringId(id), 1 },
        name,
        defaultValue);
}

[[nodiscard]] std::unique_ptr<juce::AudioParameterChoice> makeChoice(std::string_view id,
                                                                     const juce::String& name,
                                                                     const juce::StringArray& choices,
                                                                     int defaultValue)
{
    return std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { stringId(id), 1 },
        name,
        choices,
        defaultValue);
}

[[nodiscard]] float raw(const juce::AudioProcessorValueTreeState& state, std::string_view id)
{
    if (const auto* value = state.getRawParameterValue(stringId(id)); value != nullptr)
        return value->load();
    return 0.0f;
}

[[nodiscard]] bool rawBool(const juce::AudioProcessorValueTreeState& state, std::string_view id)
{
    return raw(state, id) >= 0.5f;
}

[[nodiscard]] int rawChoice(const juce::AudioProcessorValueTreeState& state, std::string_view id)
{
    return static_cast<int>(std::round(raw(state, id)));
}

template <typename Enum>
[[nodiscard]] Enum choice(const juce::AudioProcessorValueTreeState& state, std::string_view id, Enum fallback)
{
    const auto value = rawChoice(state, id);
    if (value < 0)
        return fallback;
    return static_cast<Enum>(value);
}
} // namespace

Layout createParameterLayout()
{
    Layout layout;

    juce::NormalisableRange<float> frequencyRange { 20.0f, 20000.0f, 0.01f, 0.35f };
    const auto percentAttributes = juce::AudioParameterFloatAttributes {}
        .withStringFromValueFunction([] (float value, int) { return formatFixed(value, 1) + " %"; })
        .withValueFromStringFunction([] (const juce::String& text) { return static_cast<float>(parseNumber(text)); });
    const auto decibelAttributes = juce::AudioParameterFloatAttributes {}
        .withStringFromValueFunction([] (float value, int) { return formatFixed(value, 1) + " dB"; })
        .withValueFromStringFunction([] (const juce::String& text) { return static_cast<float>(parseNumber(text)); });
    const auto frequencyAttributes = juce::AudioParameterFloatAttributes {}
        .withStringFromValueFunction([] (float value, int)
        {
            return value >= 1000.0f
                ? juce::String(value / 1000.0f, 2) + " kHz"
                : formatFixed(value, 0) + " Hz";
        })
        .withValueFromStringFunction([] (const juce::String& text) { return static_cast<float>(parseNumber(text)); });
    const auto slopeAttributes = juce::AudioParameterFloatAttributes {}
        .withStringFromValueFunction([] (float value, int) { return formatFixed(value, 1) + " dB/oct"; })
        .withValueFromStringFunction([] (const juce::String& text) { return static_cast<float>(parseNumber(text)); });
    const auto timeAttributes = juce::AudioParameterFloatAttributes {}
        .withStringFromValueFunction([] (float value, int) { return formatFixed(value, 2) + " ms"; })
        .withValueFromStringFunction([] (const juce::String& text) { return static_cast<float>(parseNumber(text)); });

    layout.add(makeFloat(ids::tkeoSharp, "TKEO Sharp", { 0.0f, 100.0f, 0.01f }, 42.0f, "%", percentAttributes));
    layout.add(makeFloat(ids::annihilation, "Annihilation", { -100.0f, 0.0f, 0.01f }, -6.0f, "dB", decibelAttributes));
    layout.add(makeFloat(ids::minFrequency, "Min Frequency", frequencyRange, 4500.0f, "Hz", frequencyAttributes));
    layout.add(makeFloat(ids::maxFrequency, "Max Frequency", frequencyRange, 12000.0f, "Hz", frequencyAttributes));
    layout.add(makeFloat(ids::cutWidth, "Cut Width", { 0.0f, 100.0f, 0.01f }, 55.0f, "%", percentAttributes));
    layout.add(makeFloat(ids::cutDepth, "Cut Depth", { 0.0f, 100.0f, 0.01f }, 80.0f, "%", percentAttributes));
    layout.add(makeFloat(ids::cutSlope, "Cut Slope", { 0.0f, 100.0f, 0.01f }, 36.0f, "dB/oct", slopeAttributes));
    layout.add(makeFloat(ids::mix, "Mix", { 0.0f, 100.0f, 0.01f }, 100.0f, "%", percentAttributes));
    layout.add(makeChoice(ids::mode, "Mode", { "Absolute", "Relative" }, 1));
    layout.add(makeChoice(ids::range, "Range", { "Split", "Wide" }, 0));
    layout.add(makeChoice(ids::filter, "Filter", { "Lowpass", "Peak" }, 1));
    layout.add(makeBool(ids::filterSolo, "Filter Solo", false));
    layout.add(makeChoice(ids::profile, "Source Mode", { "Single Vocal", "Allround" }, 0));
    layout.add(makeChoice(ids::sideChain, "Side Chain", { "Internal", "External", "MIDI" }, 0));
    layout.add(makeFloat(ids::stereoLink, "Stereo Link", { 0.0f, 100.0f, 0.01f }, 100.0f, "%", percentAttributes));
    layout.add(makeChoice(ids::stereoMode, "Stereo Link Mode", { "Mid", "Side" }, 0));
    layout.add(makeBool(ids::lookaheadEnabled, "Lookahead", false));
    layout.add(makeFloat(ids::lookaheadMs, "Lookahead Time", { 0.0f, 20.0f, 0.01f }, 0.0f, "ms", timeAttributes));
    layout.add(makeBool(ids::triggerHear, "Trigger Hear", false));
    layout.add(makeFloat(ids::inputLevel, "Input Level", { -100.0f, 100.0f, 0.01f }, 0.0f, "dB", decibelAttributes));
    layout.add(makeFloat(ids::inputPan, "Input Pan", { -100.0f, 100.0f, 0.01f }, 0.0f, "%", percentAttributes));
    layout.add(makeFloat(ids::outputLevel, "Output Level", { -100.0f, 100.0f, 0.01f }, 0.0f, "dB", decibelAttributes));
    layout.add(makeFloat(ids::outputPan, "Output Pan", { -100.0f, 100.0f, 0.01f }, 0.0f, "%", percentAttributes));
    layout.add(makeBool(ids::bypass, "FX Bypass", false));

    return layout;
}

nebula::dsp::EngineParameters readEngineParameters(const juce::AudioProcessorValueTreeState& state)
{
    nebula::dsp::EngineParameters params;
    params.tkeoSharpPercent = raw(state, ids::tkeoSharp);
    params.annihilationDb = raw(state, ids::annihilation);
    params.minFrequencyHz = raw(state, ids::minFrequency);
    params.maxFrequencyHz = raw(state, ids::maxFrequency);
    params.cutWidthPercent = raw(state, ids::cutWidth);
    params.cutDepthPercent = raw(state, ids::cutDepth);
    params.cutSlopeDbPerOct = raw(state, ids::cutSlope);
    params.mixPercent = raw(state, ids::mix);
    params.detectionMode = choice(state, ids::mode, nebula::dsp::DetectionMode::relative);
    params.rangeMode = choice(state, ids::range, nebula::dsp::RangeMode::split);
    params.filterShape = choice(state, ids::filter, nebula::dsp::FilterShape::peak);
    params.filterSolo = rawBool(state, ids::filterSolo);
    params.profileMode = choice(state, ids::profile, nebula::dsp::ProfileMode::singleVocal);
    params.sideChainMode = choice(state, ids::sideChain, nebula::dsp::SideChainMode::internal);
    params.stereoLinkPercent = raw(state, ids::stereoLink);
    params.stereoLinkMode = choice(state, ids::stereoMode, nebula::dsp::StereoLinkMode::mid);
    params.lookaheadEnabled = rawBool(state, ids::lookaheadEnabled);
    params.lookaheadMs = raw(state, ids::lookaheadMs);
    params.triggerHear = rawBool(state, ids::triggerHear);
    params.inputGainDb = raw(state, ids::inputLevel);
    params.inputPanPercent = raw(state, ids::inputPan);
    params.outputGainDb = raw(state, ids::outputLevel);
    params.outputPanPercent = raw(state, ids::outputPan);
    params.bypass = rawBool(state, ids::bypass);
    return params;
}

const std::vector<ParameterInfo>& automatableParameters()
{
    static const std::vector<ParameterInfo> infos {
        { ids::tkeoSharp, "TKEO Sharp" },
        { ids::annihilation, "Annihilation" },
        { ids::minFrequency, "Min Frequency" },
        { ids::maxFrequency, "Max Frequency" },
        { ids::cutWidth, "Cut Width" },
        { ids::cutDepth, "Cut Depth" },
        { ids::cutSlope, "Cut Slope" },
        { ids::mix, "Mix" },
        { ids::mode, "Mode" },
        { ids::range, "Range" },
        { ids::filter, "Filter" },
        { ids::filterSolo, "Filter Solo" },
        { ids::profile, "Source Mode" },
        { ids::sideChain, "Side Chain" },
        { ids::stereoLink, "Stereo Link" },
        { ids::stereoMode, "Stereo Link Mode" },
        { ids::lookaheadEnabled, "Lookahead" },
        { ids::lookaheadMs, "Lookahead Time" },
        { ids::triggerHear, "Trigger Hear" },
        { ids::inputLevel, "Input Level" },
        { ids::inputPan, "Input Pan" },
        { ids::outputLevel, "Output Level" },
        { ids::outputPan, "Output Pan" },
        { ids::bypass, "FX Bypass" }
    };
    return infos;
}

juce::String parameterName(std::string_view id)
{
    const auto& infos = automatableParameters();
    const auto it = std::find_if(infos.begin(), infos.end(), [id] (const auto& info) { return info.id == id; });
    return it == infos.end() ? stringId(id) : juce::String::fromUTF8(it->name.data(), static_cast<int>(it->name.size()));
}

float normalizedValueForText(juce::RangedAudioParameter& parameter, const juce::String& text)
{
    const auto value = parameter.getValueForText(text);
    return std::clamp(value, 0.0f, 1.0f);
}
} // namespace nebula::state
