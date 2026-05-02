#include "dsp/DeEsserEngine.h"
#include "dsp/Metrics.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numbers>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
struct StereoSignal final
{
    std::vector<double> left;
    std::vector<double> right;
};

void check(bool condition, const char* message)
{
    if (! condition)
        throw std::runtime_error(message);
}

[[nodiscard]] double sine(double phase) noexcept
{
    return std::sin(2.0 * std::numbers::pi * phase);
}

[[nodiscard]] StereoSignal makeVocalLike(double sampleRate, double seconds)
{
    const auto samples = static_cast<std::size_t>(sampleRate * seconds);
    StereoSignal signal { std::vector<double>(samples), std::vector<double>(samples) };
    std::mt19937 rng { 42U };
    std::uniform_real_distribution<double> noise(-1.0, 1.0);

    for (std::size_t i = 0; i < samples; ++i)
    {
        const auto t = static_cast<double>(i) / sampleRate;
        const auto vibrato = 1.0 + 0.012 * sine(t * 5.2);
        auto voice = 0.18 * sine(t * 220.0 * vibrato)
                   + 0.10 * sine(t * 440.0 * vibrato)
                   + 0.055 * sine(t * 660.0 * vibrato)
                   + 0.028 * sine(t * 880.0 * vibrato);

        const auto sibilantWindow = std::fmod(t + 0.08, 0.42) < 0.052 ? 1.0 : 0.0;
        const auto burstEnvelope = sibilantWindow * (0.5 - 0.5 * std::cos(std::fmod(t, 0.052) / 0.052 * 2.0 * std::numbers::pi));
        const auto air = burstEnvelope * (0.105 * noise(rng) + 0.05 * sine(t * 8200.0) + 0.035 * sine(t * 11200.0));
        const auto sample = voice + air;
        signal.left[i] = sample;
        signal.right[i] = sample * 0.992 + 0.006 * sine(t * 310.0);
    }
    return signal;
}

[[nodiscard]] StereoSignal makeCymbalLike(double sampleRate, double seconds)
{
    const auto samples = static_cast<std::size_t>(sampleRate * seconds);
    StereoSignal signal { std::vector<double>(samples), std::vector<double>(samples) };
    std::mt19937 rng { 7U };
    std::uniform_real_distribution<double> noise(-1.0, 1.0);
    double envelope = 0.0;
    for (std::size_t i = 0; i < samples; ++i)
    {
        if (i % static_cast<std::size_t>(sampleRate * 0.31) == 0U)
            envelope = 1.0;
        envelope *= 0.99955;
        const auto t = static_cast<double>(i) / sampleRate;
        const auto shimmer = 0.12 * envelope * noise(rng) + 0.04 * envelope * sine(t * 7100.0);
        signal.left[i] = shimmer;
        signal.right[i] = shimmer * 0.91 + 0.012 * envelope * sine(t * 9700.0);
    }
    return signal;
}

void copyToBuffer(const StereoSignal& signal, std::size_t offset, int blockSize, juce::AudioBuffer<double>& buffer)
{
    buffer.setSize(2, blockSize, false, false, true);
    for (int channel = 0; channel < 2; ++channel)
    {
        const auto& source = channel == 0 ? signal.left : signal.right;
        auto* destination = buffer.getWritePointer(channel);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto index = offset + static_cast<std::size_t>(sample);
            destination[sample] = index < source.size() ? source[index] : 0.0;
        }
    }
}

[[nodiscard]] StereoSignal runEngine(const StereoSignal& input,
                                     double sampleRate,
                                     int blockSize,
                                     const nebula::dsp::EngineParameters& parameters)
{
    nebula::dsp::DeEsserEngine engine;
    nebula::dsp::MeterFrame meters;
    nebula::dsp::AnalyzerExchange analyzer;
    engine.prepare(sampleRate, blockSize, 2);
    engine.setParameters(parameters);

    StereoSignal output { std::vector<double>(input.left.size()), std::vector<double>(input.right.size()) };
    juce::AudioBuffer<double> buffer;
    for (std::size_t offset = 0; offset < input.left.size(); offset += static_cast<std::size_t>(blockSize))
    {
        const auto remaining = input.left.size() - offset;
        const auto currentBlock = static_cast<int>(std::min<std::size_t>(remaining, static_cast<std::size_t>(blockSize)));
        copyToBuffer(input, offset, currentBlock, buffer);
        engine.process(buffer, nullptr, false, meters, analyzer);
        for (int sample = 0; sample < currentBlock; ++sample)
        {
            const auto index = offset + static_cast<std::size_t>(sample);
            output.left[index] = buffer.getReadPointer(0)[sample];
            output.right[index] = buffer.getReadPointer(1)[sample];
        }
    }
    return output;
}

[[nodiscard]] StereoSignal goldenReference(const StereoSignal& input, double sampleRate)
{
    StereoSignal output { input.left, input.right };
    for (std::size_t i = 1; i < output.left.size(); ++i)
    {
        const auto hpL = output.left[i] - output.left[i - 1U];
        const auto hpR = output.right[i] - output.right[i - 1U];
        const auto trigger = std::clamp((std::abs(hpL) + std::abs(hpR) - 0.035) * 16.0, 0.0, 1.0);
        const auto gain = 1.0 - trigger * 0.42;
        output.left[i] = output.left[i] - hpL * (1.0 - gain);
        output.right[i] = output.right[i] - hpR * (1.0 - gain);
    }
    juce::ignoreUnused(sampleRate);
    return output;
}

void requireFinite(const StereoSignal& signal)
{
    for (const auto value : signal.left)
        check(std::isfinite(value), "left channel produced a non-finite sample");
    for (const auto value : signal.right)
        check(std::isfinite(value), "right channel produced a non-finite sample");
}

[[nodiscard]] double maxJump(const StereoSignal& signal)
{
    double jump = 0.0;
    for (std::size_t i = 1; i < signal.left.size(); ++i)
    {
        jump = std::max(jump, std::abs(signal.left[i] - signal.left[i - 1U]));
        jump = std::max(jump, std::abs(signal.right[i] - signal.right[i - 1U]));
    }
    return jump;
}

void runAudioQualitySuite()
{
    constexpr double sampleRate = 96000.0;
    auto input = makeVocalLike(sampleRate, 2.0);

    nebula::dsp::EngineParameters params;
    params.annihilationDb = -9.0;
    params.tkeoSharpPercent = 34.0;
    params.cutDepthPercent = 82.0;
    params.mixPercent = 100.0;
    params.minFrequencyHz = 4800.0;
    params.maxFrequencyHz = 13200.0;

    auto output = runEngine(input, sampleRate, 64, params);
    requireFinite(output);

    const auto before = nebula::dsp::metrics::analyseStereo(input.left, input.right, sampleRate);
    const auto after = nebula::dsp::metrics::analyseStereo(output.left, output.right, sampleRate);
    const auto residualDb = nebula::dsp::metrics::nullResidualDb(input.left, output.left);
    const auto golden = goldenReference(input, sampleRate);
    const auto goldenDelta = nebula::dsp::metrics::nullResidualDb(golden.left, output.left);

    check(after.highBandRatio < before.highBandRatio * 0.98, "spectral balance did not reduce harsh high-band energy");
    check(after.transientScore > before.transientScore * 0.55, "transient preservation fell below target");
    check(std::abs(after.lufsIntegrated - before.lufsIntegrated) < 1.2, "LUFS stability drifted too far");
    check(after.harmonicMusicality > before.harmonicMusicality * 0.82, "harmonic musicality index regressed");
    check(std::abs(after.stereoCoherence - before.stereoCoherence) < 0.08, "stereo image coherence changed too much");
    check(residualDb < -12.0 && residualDb > -80.0, "null residual character fell outside the expected de-essing band");
    check(goldenDelta < -8.0, "golden reference comparison diverged too far");
    check(maxJump(output) < 1.0, "temporal smoothness anti-zipper test failed");

    auto bypassParams = params;
    bypassParams.bypass = true;
    const auto bypassed = runEngine(input, sampleRate, 64, bypassParams);
    check(nebula::dsp::metrics::nullResidualDb(input.left, bypassed.left) < -140.0, "soft bypass failed the null consistency check");

    std::cout << "quality before LUFS=" << before.lufsIntegrated
              << " after LUFS=" << after.lufsIntegrated
              << " highRatio=" << after.highBandRatio
              << " residual=" << residualDb
              << " goldenDelta=" << goldenDelta << '\n';
}

void runStressSuite()
{
    constexpr double sampleRate = 384000.0;
    auto input = makeVocalLike(sampleRate, 0.35);
    nebula::dsp::EngineParameters params;
    params.annihilationDb = -18.0;
    params.tkeoSharpPercent = 25.0;
    params.minFrequencyHz = 4200.0;
    params.maxFrequencyHz = 18000.0;

    for (const auto blockSize : { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048 })
    {
        const auto start = std::chrono::high_resolution_clock::now();
        auto output = runEngine(input, sampleRate, blockSize, params);
        const auto elapsed = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start).count();
        requireFinite(output);
        check(elapsed < 4.0, "buffer size timing sweep exceeded the offline guardrail");
    }

    for (const auto rate : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0, 384000.0 })
    {
        auto rateInput = makeVocalLike(rate, 0.18);
        auto output = runEngine(rateInput, rate, 37, params);
        requireFinite(output);
    }

    StereoSignal denormal { std::vector<double>(4096, 1.0e-310), std::vector<double>(4096, -1.0e-310) };
    requireFinite(runEngine(denormal, 48000.0, 17, params));

    StereoSignal silence { std::vector<double>(4096), std::vector<double>(4096) };
    const auto silentOut = runEngine(silence, 48000.0, 64, params);
    check(nebula::dsp::metrics::analyseStereo(silentOut.left, silentOut.right, 48000.0).peakDb < -140.0, "silence edge case emitted audio");

    std::mt19937 rng { 99U };
    std::uniform_real_distribution<double> random(-0.9, 0.9);
    StereoSignal fuzz { std::vector<double>(16384), std::vector<double>(16384) };
    for (std::size_t i = 0; i < fuzz.left.size(); ++i)
    {
        fuzz.left[i] = random(rng);
        fuzz.right[i] = random(rng);
    }
    requireFinite(runEngine(fuzz, 192000.0, 13, params));

    const auto first = runEngine(input, 96000.0, 64, params);
    const auto second = runEngine(input, 96000.0, 64, params);
    check(nebula::dsp::metrics::nullResidualDb(first.left, second.left) < -140.0, "null consistency test failed");

    auto cymbal = makeCymbalLike(96000.0, 0.8);
    params.profileMode = nebula::dsp::ProfileMode::allround;
    requireFinite(runEngine(cymbal, 96000.0, 128, params));
}

void runThreadAndAutomationSuite()
{
    constexpr double sampleRate = 48000.0;
    nebula::dsp::DeEsserEngine engine;
    nebula::dsp::MeterFrame meters;
    nebula::dsp::AnalyzerExchange analyzer;
    nebula::dsp::EngineParameters params;
    engine.prepare(sampleRate, 128, 2);

    std::atomic_bool running { true };
    std::thread reader([&]
    {
        std::array<float, nebula::dsp::AnalyzerExchange::windowSize> window {};
        while (running.load(std::memory_order_acquire))
            analyzer.copyWindow(window);
    });

    juce::AudioBuffer<double> buffer(2, 128);
    for (int iteration = 0; iteration < 200; ++iteration)
    {
        params.tkeoSharpPercent = static_cast<double>((iteration * 7) % 100);
        params.annihilationDb = -static_cast<double>((iteration * 11) % 60);
        params.mixPercent = static_cast<double>((iteration * 13) % 101);
        params.lookaheadEnabled = iteration % 5 == 0;
        params.lookaheadMs = static_cast<double>(iteration % 20);
        engine.setParameters(params);

        for (int channel = 0; channel < 2; ++channel)
        {
            auto* data = buffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                data[sample] = 0.2 * sine((static_cast<double>(iteration * 128 + sample) / sampleRate) * (channel == 0 ? 7000.0 : 8100.0));
        }
        engine.process(buffer, nullptr, false, meters, analyzer);
        check(std::isfinite(meters.detection.load()), "envelope tracking stability produced non-finite detection meter");
    }

    running.store(false, std::memory_order_release);
    reader.join();

    auto input = makeVocalLike(sampleRate, 0.25);
    params = {};
    const auto before = runEngine(input, sampleRate, 64, params);
    const auto afterReset = runEngine(input, sampleRate, 64, params);
    check(nebula::dsp::metrics::nullResidualDb(before.left, afterReset.left) < -140.0, "state reset consistency failed");
}
} // namespace

int main()
{
    try
    {
        runAudioQualitySuite();
        runStressSuite();
        runThreadAndAutomationSuite();
        std::cout << "Nebula J-Esser validation passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Validation failed: " << error.what() << '\n';
        return 1;
    }
}
