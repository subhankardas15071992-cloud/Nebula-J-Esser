#pragma once

#include "dsp/Analyzer.h"
#include "dsp/AtomicFloat.h"
#include "dsp/DeEsserEngine.h"
#include "state/Parameters.h"

#include <array>
#include <atomic>
#include <mutex>
#include <utility>

#include <juce_audio_processors/juce_audio_processors.h>

class NebulaJEsserAudioProcessor final : public juce::AudioProcessor
{
public:
    NebulaJEsserAudioProcessor();
    ~NebulaJEsserAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    void processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nebula J-Esser"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& state() noexcept { return parameters; }
    const juce::AudioProcessorValueTreeState& state() const noexcept { return parameters; }
    juce::UndoManager& undo() noexcept { return undoManager; }
    nebula::dsp::MeterFrame& meterFrame() noexcept { return meters; }
    nebula::dsp::AnalyzerExchange& analyzerExchange() noexcept { return analyzer; }

    void resetDetectionMax() noexcept { meters.detectionMax.store(-160.0f); }
    void resetReductionMax() noexcept { meters.reductionMax.store(0.0f); }

    void setEditorSize(int width, int height) noexcept;
    [[nodiscard]] std::pair<int, int> editorSize() const noexcept;

    void savePreset(const juce::String& name);
    void loadPreset(int index);
    [[nodiscard]] juce::StringArray presetNames() const;

    void switchAB();
    [[nodiscard]] bool isStateA() const noexcept { return activeAB.load(std::memory_order_acquire) == 0; }

    void setMidiLearnEnabled(bool shouldLearn) noexcept;
    [[nodiscard]] bool isMidiLearnEnabled() const noexcept { return midiLearnEnabled.load(std::memory_order_acquire); }
    void selectMidiLearnTarget(const juce::String& parameterId);
    void setMidiControlEnabled(bool enabled) noexcept { midiControlEnabled.store(enabled, std::memory_order_release); }
    [[nodiscard]] bool isMidiControlEnabled() const noexcept { return midiControlEnabled.load(std::memory_order_acquire); }
    void clearMidiMapping(int cc) noexcept;
    void clearAllMidiMappings() noexcept;
    void saveMidiRollbackPoint();
    void rollBackMidiMappings() noexcept;
    [[nodiscard]] juce::StringArray midiMappingDescriptions() const;

private:
    static constexpr int midiCcCount = 128;
    static constexpr int noMapping = -1;

    using AudioProcessor::processBlock;

    template <typename Sample>
    void processTypedBlock(juce::AudioBuffer<Sample>& buffer, juce::MidiBuffer& midiMessages);

    [[nodiscard]] bool consumeMidi(juce::MidiBuffer& midiMessages);
    void applyMidiCc(int cc, int value);
    [[nodiscard]] int parameterIndexForId(const juce::String& parameterId) const;
    [[nodiscard]] juce::String parameterIdForIndex(int index) const;
    [[nodiscard]] juce::ValueTree captureParameterState() const;
    void applyParameterState(const juce::ValueTree& tree);
    [[nodiscard]] juce::ValueTree midiMappingsToTree() const;
    void midiMappingsFromTree(const juce::ValueTree& tree);
    void ensureABInitialized();

    juce::UndoManager undoManager {};
    juce::AudioProcessorValueTreeState parameters;
    nebula::dsp::DeEsserEngine engine {};
    nebula::dsp::MeterFrame meters {};
    nebula::dsp::AnalyzerExchange analyzer {};

    juce::AudioBuffer<double> bridgeBuffer {};
    juce::AudioBuffer<double> bridgeSidechain {};

    std::array<std::atomic<int>, midiCcCount> midiCcToParameter {};
    std::array<std::atomic<int>, midiCcCount> savedMidiCcToParameter {};
    std::atomic_bool midiControlEnabled { true };
    std::atomic_bool midiLearnEnabled { false };
    std::atomic<int> pendingLearnParameter { noMapping };

    mutable std::mutex presetMutex {};
    juce::ValueTree presets { "PRESETS" };
    juce::ValueTree stateA { "STATE_A" };
    juce::ValueTree stateB { "STATE_B" };
    std::atomic<int> activeAB { 0 };
    int midiHeldNotes {};

    std::atomic<int> editorWidth { 1180 };
    std::atomic<int> editorHeight { 720 };
};
