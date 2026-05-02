#include "dsp/Metrics.h"

#include "dsp/Filters.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>
#include <numbers>

namespace nebula::dsp::metrics
{
namespace
{
constexpr double epsilon = 1.0e-12;

[[nodiscard]] double rms(const std::vector<double>& data)
{
    if (data.empty())
        return 0.0;

    const auto sum = std::inner_product(data.begin(), data.end(), data.begin(), 0.0);
    return std::sqrt(sum / static_cast<double>(data.size()));
}

[[nodiscard]] double peak(const std::vector<double>& data)
{
    double value = 0.0;
    for (const auto sample : data)
        value = std::max(value, std::abs(sample));
    return value;
}

[[nodiscard]] std::vector<double> monoSum(const std::vector<double>& left, const std::vector<double>& right)
{
    const auto count = std::min(left.size(), right.size());
    std::vector<double> mono(count);
    for (std::size_t i = 0; i < count; ++i)
        mono[i] = 0.5 * (left[i] + right[i]);
    return mono;
}

[[nodiscard]] std::vector<std::complex<double>> dft(const std::vector<double>& input, std::size_t bins)
{
    std::vector<std::complex<double>> output(bins);
    const auto count = input.size();
    if (count == 0)
        return output;

    for (std::size_t bin = 0; bin < bins; ++bin)
    {
        std::complex<double> acc {};
        for (std::size_t n = 0; n < count; ++n)
        {
            const auto window = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(n) / static_cast<double>(count - 1U));
            const auto phase = -2.0 * std::numbers::pi * static_cast<double>(bin * n) / static_cast<double>(count);
            acc += input[n] * window * std::complex<double> { std::cos(phase), std::sin(phase) };
        }
        output[bin] = acc;
    }
    return output;
}

[[nodiscard]] double spectralCentroid(const std::vector<double>& mono, double sampleRate)
{
    const auto fftSize = std::min<std::size_t>(2048U, mono.size());
    if (fftSize < 64U)
        return 0.0;

    std::vector<double> window(mono.end() - static_cast<std::ptrdiff_t>(fftSize), mono.end());
    const auto spectrum = dft(window, fftSize / 2U);

    double weighted = 0.0;
    double total = 0.0;
    for (std::size_t bin = 1; bin < spectrum.size(); ++bin)
    {
        const auto magnitude = std::abs(spectrum[bin]);
        const auto frequency = static_cast<double>(bin) * sampleRate / static_cast<double>(fftSize);
        weighted += frequency * magnitude;
        total += magnitude;
    }
    return total > epsilon ? weighted / total : 0.0;
}

[[nodiscard]] double highBandRatio(const std::vector<double>& mono, double sampleRate)
{
    const auto fftSize = std::min<std::size_t>(2048U, mono.size());
    if (fftSize < 64U)
        return 0.0;

    std::vector<double> window(mono.end() - static_cast<std::ptrdiff_t>(fftSize), mono.end());
    const auto spectrum = dft(window, fftSize / 2U);

    double high = 0.0;
    double total = 0.0;
    for (std::size_t bin = 1; bin < spectrum.size(); ++bin)
    {
        const auto magnitude = std::norm(spectrum[bin]);
        const auto frequency = static_cast<double>(bin) * sampleRate / static_cast<double>(fftSize);
        total += magnitude;
        if (frequency >= 5000.0)
            high += magnitude;
    }
    return total > epsilon ? high / total : 0.0;
}

[[nodiscard]] double transientScore(const std::vector<double>& mono)
{
    if (mono.size() < 3U)
        return 0.0;

    double edgeEnergy = 0.0;
    double signalEnergy = 0.0;
    for (std::size_t i = 1; i < mono.size(); ++i)
    {
        const auto delta = mono[i] - mono[i - 1U];
        edgeEnergy += delta * delta;
        signalEnergy += mono[i] * mono[i];
    }
    return edgeEnergy / std::max(signalEnergy, epsilon);
}

[[nodiscard]] double harmonicMusicality(const std::vector<double>& mono, double sampleRate)
{
    const auto fftSize = std::min<std::size_t>(4096U, mono.size());
    if (fftSize < 256U)
        return 0.0;

    std::vector<double> window(mono.end() - static_cast<std::ptrdiff_t>(fftSize), mono.end());
    const auto spectrum = dft(window, fftSize / 2U);

    auto maxIt = std::max_element(spectrum.begin() + 1, spectrum.end(),
        [] (auto a, auto b) { return std::norm(a) < std::norm(b); });

    const auto fundamentalBin = static_cast<std::size_t>(std::distance(spectrum.begin(), maxIt));
    if (fundamentalBin == 0U)
        return 0.0;

    double harmonic = 0.0;
    double total = 0.0;
    for (std::size_t bin = 1; bin < spectrum.size(); ++bin)
    {
        const auto power = std::norm(spectrum[bin]);
        total += power;
        const auto harmonicIndex = static_cast<double>(bin) / static_cast<double>(fundamentalBin);
        const auto nearest = std::round(harmonicIndex);
        const auto frequency = static_cast<double>(bin) * sampleRate / static_cast<double>(fftSize);
        if (nearest >= 1.0 && std::abs(harmonicIndex - nearest) < 0.035 && frequency < 16000.0)
            harmonic += power;
    }
    return total > epsilon ? harmonic / total : 0.0;
}
} // namespace

AudioMetrics analyseStereo(const std::vector<double>& left, const std::vector<double>& right, double sampleRate)
{
    auto mono = monoSum(left, right);
    const auto monoRms = rms(mono);

    double cross = 0.0;
    double leftEnergy = 0.0;
    double rightEnergy = 0.0;
    const auto count = std::min(left.size(), right.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        cross += left[i] * right[i];
        leftEnergy += left[i] * left[i];
        rightEnergy += right[i] * right[i];
    }

    AudioMetrics result {};
    result.rmsDb = gainToDb(monoRms);
    result.peakDb = gainToDb(std::max(peak(left), peak(right)));
    result.lufsIntegrated = -0.691 + 10.0 * std::log10(std::max(monoRms * monoRms, epsilon));
    result.spectralCentroidHz = spectralCentroid(mono, sampleRate);
    result.highBandRatio = highBandRatio(mono, sampleRate);
    result.transientScore = transientScore(mono);
    result.harmonicMusicality = harmonicMusicality(mono, sampleRate);
    result.stereoCoherence = cross / std::sqrt(std::max(leftEnergy * rightEnergy, epsilon));
    result.nullResidualDb = 0.0;
    return result;
}

double nullResidualDb(const std::vector<double>& reference, const std::vector<double>& test)
{
    const auto count = std::min(reference.size(), test.size());
    if (count == 0U)
        return -160.0;

    double residual = 0.0;
    double baseline = 0.0;
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto delta = reference[i] - test[i];
        residual += delta * delta;
        baseline += reference[i] * reference[i];
    }

    return 10.0 * std::log10(std::max(residual, epsilon) / std::max(baseline, epsilon));
}
} // namespace nebula::dsp::metrics
