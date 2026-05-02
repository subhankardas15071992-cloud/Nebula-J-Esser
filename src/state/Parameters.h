#pragma once

#include "dsp/DeEsserEngine.h"

#include <string_view>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

namespace nebula::state
{
namespace ids
{
inline constexpr std::string_view tkeoSharp { "tkeoSharp" };
inline constexpr std::string_view annihilation { "annihilation" };
inline constexpr std::string_view minFrequency { "minFrequency" };
inline constexpr std::string_view maxFrequency { "maxFrequency" };
inline constexpr std::string_view cutWidth { "cutWidth" };
inline constexpr std::string_view cutDepth { "cutDepth" };
inline constexpr std::string_view cutSlope { "cutSlope" };
inline constexpr std::string_view mix { "mix" };
inline constexpr std::string_view mode { "mode" };
inline constexpr std::string_view range { "range" };
inline constexpr std::string_view filter { "filter" };
inline constexpr std::string_view filterSolo { "filterSolo" };
inline constexpr std::string_view profile { "profile" };
inline constexpr std::string_view sideChain { "sideChain" };
inline constexpr std::string_view stereoLink { "stereoLink" };
inline constexpr std::string_view stereoMode { "stereoMode" };
inline constexpr std::string_view lookaheadEnabled { "lookaheadEnabled" };
inline constexpr std::string_view lookaheadMs { "lookaheadMs" };
inline constexpr std::string_view triggerHear { "triggerHear" };
inline constexpr std::string_view inputLevel { "inputLevel" };
inline constexpr std::string_view inputPan { "inputPan" };
inline constexpr std::string_view outputLevel { "outputLevel" };
inline constexpr std::string_view outputPan { "outputPan" };
inline constexpr std::string_view bypass { "bypass" };
} // namespace ids

struct ParameterInfo final
{
    std::string_view id {};
    std::string_view name {};
};

[[nodiscard]] juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
[[nodiscard]] nebula::dsp::EngineParameters readEngineParameters(const juce::AudioProcessorValueTreeState& state);
[[nodiscard]] const std::vector<ParameterInfo>& automatableParameters();
[[nodiscard]] juce::String parameterName(std::string_view id);
[[nodiscard]] float normalizedValueForText(juce::RangedAudioParameter& parameter, const juce::String& text);
} // namespace nebula::state
