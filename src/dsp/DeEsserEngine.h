#pragma once

#include "dsp/Analyzer.h"
#include "dsp/AtomicFloat.h"
#include "dsp/Filters.h"

#include <array>
#include <utility>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

namespace nebula::dsp
{
enum class DetectionMode
{
    absolute = 0,
    relative
};

enum class RangeMode
{
    split = 0,
    wide
};

enum class FilterShape
{
    lowpass = 0,
    peak
};

enum class ProfileMode
{
    singleVocal = 0,
    allround
};

enum class SideChainMode
{
    internal = 0,
    external,
    midi
};

enum class StereoLinkMode
{
    mid = 0,
    side
};

struct EngineParameters final
{
    double tkeoSharpPercent { 42.0 };
    double annihilationDb { -6.0 };
    double minFrequencyHz { 4500.0 };
    double maxFrequencyHz { 12000.0 };
    double cutWidthPercent { 55.0 };
    double cutDepthPercent { 80.0 };
    double cutSlopeDbPerOct { 36.0 };
    double mixPercent { 100.0 };
    DetectionMode detectionMode { DetectionMode::relative };
    RangeMode rangeMode { RangeMode::split };
    FilterShape filterShape { FilterShape::peak };
    bool filterSolo { false };
    ProfileMode profileMode { ProfileMode::singleVocal };
    SideChainMode sideChainMode { SideChainMode::internal };
    double stereoLinkPercent { 100.0 };
    StereoLinkMode stereoLinkMode { StereoLinkMode::mid };
    bool lookaheadEnabled { false };
    double lookaheadMs { 0.0 };
    bool triggerHear { false };
    double inputGainDb { 0.0 };
    double inputPanPercent { 0.0 };
    double outputGainDb { 0.0 };
    double outputPanPercent { 0.0 };
    bool bypass { false };
};

class DeEsserEngine final
{
public:
    void prepare(double newSampleRate, int maxBlockSize, int channels);
    void reset();
    void setParameters(const EngineParameters& newParameters);

    [[nodiscard]] int getLatencySamples() const noexcept { return latencySamples; }

    void process(juce::AudioBuffer<double>& buffer,
                 const juce::AudioBuffer<double>* externalSidechain,
                 bool midiTriggered,
                 MeterFrame& meters,
                 AnalyzerExchange& analyzer);

private:
    static constexpr int maxChannels = 64;
    static constexpr int bellStages = 3;

    struct LookaheadLine final
    {
        void prepare(int maxDelaySamples);
        void reset() noexcept;
        [[nodiscard]] double pushPop(double input, int delaySamples) noexcept;

        std::vector<double> buffer {};
        int write {};
    };

    struct ChannelState final
    {
        void prepare(double sampleRate, int maxDelaySamples);
        void reset() noexcept;
        void configure(const EngineParameters& params, double sampleRate);

        ButterworthSixth detectionHighpass {};
        ButterworthSixth detectionLowpass {};
        OnePoleTptLowpass splitLowAtMin {};
        OnePoleTptLowpass splitLowAtMax {};
        std::array<DynamicBellTpt, bellStages> wideBells {};
        TkeoDetector tkeo {};
        ThreeStageSmoother gainSmoother {};
        ThreeStageSmoother bypassSmoother {};
        LookaheadLine lookahead {};
        double slowVoice {};
        double fastAir {};
        double prevBand {};
        double lastDetection {};
    };

    [[nodiscard]] double detect(ChannelState& state, double sidechainSample) const noexcept;
    [[nodiscard]] double processSplit(ChannelState& state, double input, double gain, double bridgeAmount) noexcept;
    [[nodiscard]] double processWide(ChannelState& state, double input, double gain) noexcept;
    [[nodiscard]] double harmonicBridge(ChannelState& state, double dryBand, double removedBand) const noexcept;
    [[nodiscard]] std::pair<double, double> panGains(double panPercent) const noexcept;
    void updateLatency();

    EngineParameters params {};
    double sampleRate { 48000.0 };
    int latencySamples {};
    bool parameterStatePrimed { false };
    std::array<ChannelState, maxChannels> channels {};
};
} // namespace nebula::dsp
