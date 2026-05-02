#include "dsp/DeEsserEngine.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace nebula::dsp
{
namespace
{
[[nodiscard]] double smoothStep(double edge0, double edge1, double x) noexcept
{
    const auto t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1.0e-9), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

[[nodiscard]] double percentageToQ(double widthPercent) noexcept
{
    const auto normalized = std::clamp(widthPercent / 100.0, 0.0, 1.0);
    return 0.35 + normalized * normalized * 23.65;
}

[[nodiscard]] double midiTriggerEnvelope(bool triggered) noexcept
{
    return triggered ? 1.0 : 0.0;
}

[[nodiscard]] double peakFrequency(double minFrequency, double maxFrequency) noexcept
{
    return std::sqrt(std::max(20.0, minFrequency) * std::max(20.0, maxFrequency));
}

[[nodiscard]] double absolute(double value) noexcept
{
    return std::abs(value);
}
} // namespace

void DeEsserEngine::LookaheadLine::prepare(int maxDelaySamples)
{
    buffer.assign(static_cast<std::size_t>(std::max(1, maxDelaySamples + 2)), 0.0);
    write = 0;
}

void DeEsserEngine::LookaheadLine::reset() noexcept
{
    std::fill(buffer.begin(), buffer.end(), 0.0);
    write = 0;
}

double DeEsserEngine::LookaheadLine::pushPop(double input, int delaySamples) noexcept
{
    if (buffer.empty() || delaySamples <= 0)
        return input;

    const auto size = static_cast<int>(buffer.size());
    const auto clampedDelay = std::clamp(delaySamples, 0, size - 1);
    auto read = write - clampedDelay;
    if (read < 0)
        read += size;

    const auto output = buffer[static_cast<std::size_t>(read)];
    buffer[static_cast<std::size_t>(write)] = input;
    write = (write + 1) % size;
    return output;
}

void DeEsserEngine::ChannelState::prepare(double newSampleRate, int maxDelaySamples)
{
    detectionHighpass.prepare(newSampleRate);
    detectionLowpass.prepare(newSampleRate);
    splitLowAtMin.prepare(newSampleRate);
    splitLowAtMax.prepare(newSampleRate);
    for (auto& bell : wideBells)
        bell.prepare(newSampleRate);
    gainSmoother.prepare(newSampleRate, 0.12);
    bypassSmoother.prepare(newSampleRate, 1.8);
    lookahead.prepare(maxDelaySamples);
    reset();
}

void DeEsserEngine::ChannelState::reset() noexcept
{
    detectionHighpass.reset();
    detectionLowpass.reset();
    splitLowAtMin.reset();
    splitLowAtMax.reset();
    for (auto& bell : wideBells)
        bell.reset();
    tkeo.reset();
    gainSmoother.reset(1.0);
    bypassSmoother.reset(1.0);
    lookahead.reset();
    slowVoice = 0.0;
    fastAir = 0.0;
    prevBand = 0.0;
    lastDetection = 0.0;
}

void DeEsserEngine::ChannelState::configure(const EngineParameters& p, double newSampleRate)
{
    const auto minFrequency = std::min(p.minFrequencyHz, p.maxFrequencyHz);
    const auto maxFrequency = std::max(p.minFrequencyHz, p.maxFrequencyHz);
    detectionHighpass.set(ButterworthSixth::Type::highpass, minFrequency);
    detectionLowpass.set(ButterworthSixth::Type::lowpass, maxFrequency);
    splitLowAtMin.setCutoff(minFrequency);
    splitLowAtMax.setCutoff(maxFrequency);

    const auto q = percentageToQ(p.cutWidthPercent);
    const auto center = clampFrequency(peakFrequency(minFrequency, maxFrequency), newSampleRate);
    for (auto& bell : wideBells)
        bell.setShape(center, q, p.cutSlopeDbPerOct);
}

void DeEsserEngine::prepare(double newSampleRate, int maxBlockSize, int channelCount)
{
    sampleRate = std::clamp(newSampleRate, 1000.0, 384000.0);
    const auto preparedBlockSize = std::max(1, maxBlockSize);
    const auto maxDelaySamples = static_cast<int>(std::ceil(sampleRate * 0.020)) + preparedBlockSize + 4;

    for (auto& channel : channels)
        channel.prepare(sampleRate, maxDelaySamples);

    parameterStatePrimed = false;
    updateLatency();
}

void DeEsserEngine::reset()
{
    for (auto& channel : channels)
        channel.reset();
}

void DeEsserEngine::setParameters(const EngineParameters& newParameters)
{
    params = newParameters;

    if (! parameterStatePrimed)
    {
        const auto bypassMix = params.bypass ? 0.0 : 1.0;
        for (auto& channel : channels)
            channel.bypassSmoother.reset(bypassMix);
        parameterStatePrimed = true;
    }

    updateLatency();
}

void DeEsserEngine::updateLatency()
{
    latencySamples = params.lookaheadEnabled
        ? static_cast<int>(std::round(std::clamp(params.lookaheadMs, 0.0, 20.0) * sampleRate * 0.001))
        : 0;
}

double DeEsserEngine::detect(ChannelState& state, double sidechainSample) const noexcept
{
    auto filtered = params.filterShape == FilterShape::lowpass
        ? state.detectionLowpass.process(sidechainSample)
        : state.detectionLowpass.process(state.detectionHighpass.process(sidechainSample));

    if (params.sideChainMode == SideChainMode::midi)
        filtered = sidechainSample;
    state.lastDetection = filtered;

    const auto tkeoEnergy = state.tkeo.process(filtered);
    const auto absFiltered = absolute(filtered);
    state.slowVoice = 0.9992 * state.slowVoice + 0.0008 * absFiltered;
    state.fastAir = 0.94 * state.fastAir + 0.06 * absolute(filtered - state.prevBand);
    state.prevBand = filtered;

    const auto profileBias = params.profileMode == ProfileMode::singleVocal ? 1.22 : 0.92;
    const auto threshold = (0.16 + 1.84 * std::clamp(params.tkeoSharpPercent / 100.0, 0.0, 1.0)) * profileBias;
    auto activation = smoothStep(threshold, threshold * 3.0 + 1.0e-9, tkeoEnergy);

    if (params.detectionMode == DetectionMode::relative)
    {
        const auto contextualContrast = state.fastAir / std::max(state.slowVoice, 1.0e-6);
        const auto adaptiveVectors = 0.55 + 0.45 * smoothStep(1.0, 14.0, contextualContrast);
        activation *= adaptiveVectors;
    }

    return std::clamp(activation, 0.0, 1.0);
}

double DeEsserEngine::harmonicBridge(ChannelState& state, double dryBand, double removedBand) const noexcept
{
    const auto phaseReference = std::tanh(state.slowVoice * 4.0);
    const auto bridge = std::tanh((dryBand - removedBand) * phaseReference * 0.35);
    return bridge * 0.018 * std::clamp(params.mixPercent / 100.0, 0.0, 1.0);
}

double DeEsserEngine::processSplit(ChannelState& state, double input, double gain, double bridgeAmount) noexcept
{
    const auto lowAtMax = state.splitLowAtMax.process(input);
    const auto lowAtMin = state.splitLowAtMin.process(input);
    const auto band = lowAtMax - lowAtMin;
    const auto residue = input - band;
    const auto attenuatedBand = band * gain;
    return residue + attenuatedBand + harmonicBridge(state, band, attenuatedBand) * bridgeAmount;
}

double DeEsserEngine::processWide(ChannelState& state, double input, double gain) noexcept
{
    auto output = input;
    const auto slope = std::clamp(params.cutSlopeDbPerOct / 100.0, 0.0, 1.0);
    const std::array<double, bellStages> weights { 1.0, slope * 0.65, slope * slope * 0.45 };
    for (int stage = 0; stage < bellStages; ++stage)
    {
        const auto stageGain = std::pow(std::clamp(gain, 0.0, 1.0), weights[static_cast<std::size_t>(stage)]);
        output = state.wideBells[static_cast<std::size_t>(stage)].process(output, stageGain);
    }
    return output;
}

std::pair<double, double> DeEsserEngine::panGains(double panPercent) const noexcept
{
    const auto pan = std::clamp(panPercent / 100.0, -1.0, 1.0);
    const auto left = pan <= 0.0 ? 1.0 : std::cos(pan * std::numbers::pi * 0.5);
    const auto right = pan >= 0.0 ? 1.0 : std::cos(-pan * std::numbers::pi * 0.5);
    return { left, right };
}

void DeEsserEngine::process(juce::AudioBuffer<double>& buffer,
                            const juce::AudioBuffer<double>* externalSidechain,
                            bool midiTriggered,
                            MeterFrame& meters,
                            AnalyzerExchange& analyzer)
{
    const auto totalSamples = buffer.getNumSamples();
    const auto totalChannels = std::min(buffer.getNumChannels(), maxChannels);
    if (totalSamples <= 0 || totalChannels <= 0)
        return;

    for (int channel = 0; channel < totalChannels; ++channel)
        channels[static_cast<std::size_t>(channel)].configure(params, sampleRate);

    const auto inputGain = dbToGain(params.inputGainDb);
    const auto outputGain = dbToGain(params.outputGainDb);
    const auto mix = std::clamp(params.mixPercent / 100.0, 0.0, 1.0);
    const auto depth = std::clamp(params.cutDepthPercent / 100.0, 0.0, 1.0);
    const auto annihilationDb = std::clamp(params.annihilationDb, -100.0, 0.0);
    const auto midiEnvelope = midiTriggerEnvelope(midiTriggered);
    const auto inputPan = panGains(params.inputPanPercent);
    const auto outputPan = panGains(params.outputPanPercent);
    const auto link = std::clamp(params.stereoLinkPercent / 100.0, 0.0, 1.0);

    double detectionPeak = 0.0;
    double reductionPeak = 0.0;
    std::array<double, maxChannels> activation {};
    std::array<double, maxChannels> rawInputSamples {};
    std::array<double, maxChannels> sidechainSamples {};

    for (int sample = 0; sample < totalSamples; ++sample)
    {
        for (int channel = 0; channel < totalChannels; ++channel)
        {
            const auto ch = static_cast<std::size_t>(channel);
            auto* data = buffer.getWritePointer(channel);
            rawInputSamples[ch] = data[sample];
            auto input = rawInputSamples[ch] * inputGain;

            if (totalChannels >= 2)
            {
                if (channel == 0)
                    input *= inputPan.first;
                else if (channel == 1)
                    input *= inputPan.second;
            }

            data[sample] = input;

            auto side = input;
            if (params.sideChainMode == SideChainMode::external
                && externalSidechain != nullptr
                && externalSidechain->getNumChannels() > 0
                && sample < externalSidechain->getNumSamples())
            {
                const auto sideChannel = std::min(channel, externalSidechain->getNumChannels() - 1);
                side = externalSidechain->getReadPointer(sideChannel)[sample];
            }
            else if (params.sideChainMode == SideChainMode::midi)
            {
                side = midiEnvelope;
            }

            sidechainSamples[ch] = side;
            activation[ch] = detect(channels[ch], side);
            detectionPeak = std::max(detectionPeak, absolute(channels[ch].lastDetection));
        }

        if (totalChannels >= 2 && link > 0.0)
        {
            if (params.stereoLinkMode == StereoLinkMode::mid)
            {
                const auto linked = std::max(activation[0], activation[1]);
                activation[0] = activation[0] * (1.0 - link) + linked * link;
                activation[1] = activation[1] * (1.0 - link) + linked * link;
            }
            else
            {
                const auto midActivation = 0.5 * (activation[0] + activation[1]);
                const auto sideActivation = std::abs(activation[0] - activation[1]);
                const auto linked = std::max(midActivation, sideActivation);
                activation[0] = activation[0] * (1.0 - link) + linked * link;
                activation[1] = activation[1] * (1.0 - link) + linked * link;
            }
        }

        for (int channel = 0; channel < totalChannels; ++channel)
        {
            const auto ch = static_cast<std::size_t>(channel);
            auto& state = channels[ch];
            auto* data = buffer.getWritePointer(channel);
            const auto delayedDry = state.lookahead.pushPop(data[sample], latencySamples);
            const auto reductionDb = annihilationDb * depth * activation[ch];
            const auto targetGain = dbToGain(reductionDb);
            const auto smoothedGain = state.gainSmoother.process(targetGain);

            auto wet = delayedDry;
            if (params.triggerHear || params.filterSolo)
            {
                wet = state.lastDetection;
            }
            else if (params.rangeMode == RangeMode::split)
            {
                wet = processSplit(state, delayedDry, smoothedGain, 1.0 - smoothedGain);
            }
            else
            {
                wet = processWide(state, delayedDry, smoothedGain);
            }

            auto output = delayedDry + (wet - delayedDry) * mix;
            output *= outputGain;
            if (totalChannels >= 2)
            {
                if (channel == 0)
                    output *= outputPan.first;
                else if (channel == 1)
                    output *= outputPan.second;
            }

            const auto activeMix = state.bypassSmoother.process(params.bypass ? 0.0 : 1.0);
            data[sample] = rawInputSamples[ch] + (output - rawInputSamples[ch]) * activeMix;
            reductionPeak = std::max(reductionPeak, -reductionDb);
        }
    }

    const auto detectionDb = static_cast<float>(gainToDb(detectionPeak));
    const auto reductionDb = static_cast<float>(reductionPeak);
    meters.detection.store(detectionDb);
    meters.reduction.store(reductionDb);
    meters.detectionMax.store(std::max(meters.detectionMax.load(), detectionDb));
    meters.reductionMax.store(std::max(meters.reductionMax.load(), reductionDb));
    analyzer.pushPostFxBlock(buffer);
}
} // namespace nebula::dsp
