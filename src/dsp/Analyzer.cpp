#include "dsp/Analyzer.h"

#include <algorithm>

namespace nebula::dsp
{
void AnalyzerExchange::reset() noexcept
{
    if (const std::unique_lock lock { mutex, std::try_to_lock }; lock.owns_lock())
    {
        ring.fill(0.0f);
        writeIndex = 0;
        decimator = 0;
    }
}

void AnalyzerExchange::pushPostFxBlock(const juce::AudioBuffer<double>& buffer) noexcept
{
    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        return;

    const std::unique_lock lock { mutex, std::try_to_lock };
    if (! lock.owns_lock())
        return;

    const auto samples = buffer.getNumSamples();
    const auto channels = buffer.getNumChannels();

    for (int sample = 0; sample < samples; ++sample)
    {
        if (++decimator < 2)
            continue;

        decimator = 0;
        double mono = 0.0;
        for (int channel = 0; channel < channels; ++channel)
            mono += buffer.getReadPointer(channel)[sample];

        ring[static_cast<std::size_t>(writeIndex)] = static_cast<float>(mono / static_cast<double>(channels));
        writeIndex = (writeIndex + 1) % windowSize;
    }
}

bool AnalyzerExchange::copyWindow(std::array<float, windowSize>& destination) const
{
    const std::lock_guard lock { mutex };
    for (int i = 0; i < windowSize; ++i)
    {
        const auto source = (writeIndex + i) % windowSize;
        destination[static_cast<std::size_t>(i)] = ring[static_cast<std::size_t>(source)];
    }
    return true;
}
} // namespace nebula::dsp
