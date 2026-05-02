#pragma once

#include <array>
#include <mutex>

#include <juce_audio_basics/juce_audio_basics.h>

namespace nebula::dsp
{
class AnalyzerExchange final
{
public:
    static constexpr int windowSize = 2048;

    void reset() noexcept;
    void pushPostFxBlock(const juce::AudioBuffer<double>& buffer) noexcept;
    [[nodiscard]] bool copyWindow(std::array<float, windowSize>& destination) const;

private:
    mutable std::mutex mutex;
    std::array<float, windowSize> ring {};
    int writeIndex {};
    int decimator {};
};
} // namespace nebula::dsp
