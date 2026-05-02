#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace nebula::dsp
{
inline double clampFrequency(double frequency, double sampleRate) noexcept
{
    const auto nyquist = std::max(20.0, sampleRate * 0.499);
    return std::clamp(frequency, 5.0, nyquist);
}

inline double dbToGain(double decibels) noexcept
{
    if (decibels <= -160.0)
        return 0.0;
    return std::pow(10.0, decibels / 20.0);
}

inline double gainToDb(double gain) noexcept
{
    return 20.0 * std::log10(std::max(gain, 1.0e-12));
}

class OnePoleTptLowpass final
{
public:
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(1000.0, newSampleRate);
        setCutoff(cutoff);
        reset();
    }

    void reset() noexcept { state = 0.0; }

    void setCutoff(double frequency) noexcept
    {
        cutoff = clampFrequency(frequency, sampleRate);
        const auto g = std::tan(std::numbers::pi * cutoff / sampleRate);
        coefficient = g / (1.0 + g);
    }

    [[nodiscard]] double process(double input) noexcept
    {
        const auto v = (input - state) * coefficient;
        const auto low = v + state;
        state = low + v;
        return low;
    }

private:
    double sampleRate { 48000.0 };
    double cutoff { 1000.0 };
    double coefficient { 0.06151176850362156 };
    double state { 0.0 };
};

class TptSvf final
{
public:
    void prepare(double newSampleRate) noexcept
    {
        sampleRate = std::max(1000.0, newSampleRate);
        update();
        reset();
    }

    void reset() noexcept
    {
        ic1 = 0.0;
        ic2 = 0.0;
    }

    void set(double newFrequency, double newQ) noexcept
    {
        frequency = clampFrequency(newFrequency, sampleRate);
        q = std::clamp(newQ, 0.1, 24.0);
        update();
    }

    struct Output final
    {
        double low {};
        double band {};
        double high {};
    };

    [[nodiscard]] Output process(double input) noexcept
    {
        const auto v3 = input - ic2;
        const auto v1 = a1 * ic1 + a2 * v3;
        const auto v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return { v2, v1, input - k * v1 - v2 };
    }

private:
    void update() noexcept
    {
        g = std::tan(std::numbers::pi * frequency / sampleRate);
        k = 1.0 / q;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    double sampleRate { 48000.0 };
    double frequency { 1000.0 };
    double q { 0.7071067811865476 };
    double g {};
    double k {};
    double a1 {};
    double a2 {};
    double a3 {};
    double ic1 {};
    double ic2 {};
};

class ButterworthSixth final
{
public:
    enum class Type
    {
        lowpass,
        highpass
    };

    void prepare(double newSampleRate) noexcept
    {
        for (auto& section : sections)
            section.prepare(newSampleRate);
        update();
        reset();
    }

    void reset() noexcept
    {
        for (auto& section : sections)
            section.reset();
    }

    void set(Type newType, double newFrequency) noexcept
    {
        type = newType;
        frequency = newFrequency;
        update();
    }

    [[nodiscard]] double process(double input) noexcept
    {
        auto value = input;
        for (auto& section : sections)
        {
            const auto out = section.process(value);
            value = type == Type::lowpass ? out.low : out.high;
        }
        return value;
    }

private:
    void update() noexcept
    {
        static constexpr std::array<double, 3> qValues {
            0.5176380902050415,
            0.7071067811865476,
            1.9318516525781366
        };

        for (std::size_t index = 0; index < sections.size(); ++index)
            sections[index].set(frequency, qValues[index]);
    }

    Type type { Type::lowpass };
    double frequency { 1000.0 };
    std::array<TptSvf, 3> sections {};
};

class ThreeStageSmoother final
{
public:
    void prepare(double newSampleRate, double timeMs) noexcept
    {
        sampleRate = std::max(1000.0, newSampleRate);
        setTime(timeMs);
        reset(0.0);
    }

    void setTime(double timeMs) noexcept
    {
        const auto samples = std::max(1.0, timeMs * 0.001 * sampleRate);
        coefficient = std::exp(-1.0 / samples);
    }

    void reset(double value) noexcept
    {
        z1 = value;
        z2 = value;
        z3 = value;
    }

    [[nodiscard]] double process(double target) noexcept
    {
        z1 += (1.0 - coefficient) * (target - z1);
        z2 += (1.0 - coefficient) * (z1 - z2);
        z3 += (1.0 - coefficient) * (z2 - z3);
        return z3;
    }

private:
    double sampleRate { 48000.0 };
    double coefficient { 0.99 };
    double z1 {};
    double z2 {};
    double z3 {};
};

class TkeoDetector final
{
public:
    void reset() noexcept
    {
        x2 = 0.0;
        x1 = 0.0;
        energy = 1.0e-9;
    }

    [[nodiscard]] double process(double x0) noexcept
    {
        const auto tkeo = std::max(0.0, x1 * x1 - x2 * x0);
        energy = 0.995 * energy + 0.005 * (x1 * x1 + 1.0e-12);
        x2 = x1;
        x1 = x0;
        return tkeo / energy;
    }

private:
    double x2 {};
    double x1 {};
    double energy { 1.0e-9 };
};

class DynamicBellTpt final
{
public:
    void prepare(double newSampleRate) noexcept
    {
        svf.prepare(newSampleRate);
        smoother.prepare(newSampleRate, 0.18);
        smoother.reset(1.0);
    }

    void reset() noexcept
    {
        svf.reset();
        smoother.reset(1.0);
    }

    void setShape(double frequency, double q, double slopeDbPerOct) noexcept
    {
        const auto slopeScale = std::clamp(slopeDbPerOct / 100.0, 0.0, 1.0);
        svf.set(frequency, std::clamp(q * (1.0 + slopeScale * 3.0), 0.2, 24.0));
    }

    [[nodiscard]] double process(double input, double targetGain) noexcept
    {
        const auto out = svf.process(input);
        const auto gain = smoother.process(targetGain);
        return out.low + out.high + out.band * gain;
    }

private:
    TptSvf svf {};
    ThreeStageSmoother smoother {};
};
} // namespace nebula::dsp
