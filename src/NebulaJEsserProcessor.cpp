#include "NebulaJEsserProcessor.h"

#include "NebulaJEsserEditor.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

NebulaJEsserAudioProcessor::NebulaJEsserAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, &undoManager, "PARAMETERS", nebula::state::createParameterLayout())
{
    clearAllMidiMappings();
    saveMidiRollbackPoint();
    ensureABInitialized();
}

void NebulaJEsserAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const auto channels = std::max(1, getTotalNumOutputChannels());
    midiHeldNotes = 0;
    engine.prepare(sampleRate, samplesPerBlock, channels);
    engine.setParameters(nebula::state::readEngineParameters(parameters));
    setLatencySamples(engine.getLatencySamples());
    bridgeBuffer.setSize(channels, samplesPerBlock, false, false, true);
    bridgeSidechain.setSize(channels, samplesPerBlock, false, false, true);
    analyzer.reset();
}

void NebulaJEsserAudioProcessor::releaseResources()
{
    bridgeBuffer.setSize(0, 0);
    bridgeSidechain.setSize(0, 0);
}

bool NebulaJEsserAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainIn != mainOut)
        return false;

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto side = layouts.getChannelSet(true, 1);
        if (! side.isDisabled() && side != juce::AudioChannelSet::mono() && side != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

void NebulaJEsserAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    processTypedBlock(buffer, midiMessages);
}

void NebulaJEsserAudioProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    processTypedBlock(buffer, midiMessages);
}

template <typename Sample>
void NebulaJEsserAudioProcessor::processTypedBlock(juce::AudioBuffer<Sample>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const auto midiTriggered = consumeMidi(midiMessages);

    auto mainBuffer = getBusBuffer(buffer, false, 0);
    auto sidechainBuffer = getBusCount(true) > 1 && getChannelCountOfBus(true, 1) > 0
        ? getBusBuffer(buffer, true, 1)
        : juce::AudioBuffer<Sample> {};

    engine.setParameters(nebula::state::readEngineParameters(parameters));
    if (engine.getLatencySamples() != getLatencySamples())
        setLatencySamples(engine.getLatencySamples());

    if constexpr (std::is_same_v<Sample, double>)
    {
        const juce::AudioBuffer<double>* sidechain = sidechainBuffer.getNumChannels() > 0 ? &sidechainBuffer : nullptr;
        engine.process(mainBuffer, sidechain, midiTriggered, meters, analyzer);
    }
    else
    {
        bridgeBuffer.setSize(mainBuffer.getNumChannels(), mainBuffer.getNumSamples(), false, false, true);
        for (int channel = 0; channel < mainBuffer.getNumChannels(); ++channel)
        {
            const auto* source = mainBuffer.getReadPointer(channel);
            auto* destination = bridgeBuffer.getWritePointer(channel);
            for (int sample = 0; sample < mainBuffer.getNumSamples(); ++sample)
                destination[sample] = static_cast<double>(source[sample]);
        }

        const juce::AudioBuffer<double>* sidechain = nullptr;
        if (sidechainBuffer.getNumChannels() > 0)
        {
            bridgeSidechain.setSize(sidechainBuffer.getNumChannels(), sidechainBuffer.getNumSamples(), false, false, true);
            for (int channel = 0; channel < sidechainBuffer.getNumChannels(); ++channel)
            {
                const auto* source = sidechainBuffer.getReadPointer(channel);
                auto* destination = bridgeSidechain.getWritePointer(channel);
                for (int sample = 0; sample < sidechainBuffer.getNumSamples(); ++sample)
                    destination[sample] = static_cast<double>(source[sample]);
            }
            sidechain = &bridgeSidechain;
        }

        engine.process(bridgeBuffer, sidechain, midiTriggered, meters, analyzer);

        for (int channel = 0; channel < mainBuffer.getNumChannels(); ++channel)
        {
            const auto* source = bridgeBuffer.getReadPointer(channel);
            auto* destination = mainBuffer.getWritePointer(channel);
            for (int sample = 0; sample < mainBuffer.getNumSamples(); ++sample)
                destination[sample] = static_cast<float>(source[sample]);
        }
    }
}

juce::AudioProcessorEditor* NebulaJEsserAudioProcessor::createEditor()
{
    return new NebulaJEsserAudioProcessorEditor(*this);
}

void NebulaJEsserAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String NebulaJEsserAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void NebulaJEsserAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

bool NebulaJEsserAudioProcessor::consumeMidi(juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        if (message.isNoteOn())
            ++midiHeldNotes;
        else if (message.isNoteOff())
            midiHeldNotes = std::max(0, midiHeldNotes - 1);

        if (message.isController())
            applyMidiCc(message.getControllerNumber(), message.getControllerValue());
    }
    return midiHeldNotes > 0;
}

void NebulaJEsserAudioProcessor::applyMidiCc(int cc, int value)
{
    if (cc < 0 || cc >= midiCcCount)
        return;

    const auto pending = pendingLearnParameter.load(std::memory_order_acquire);
    if (midiLearnEnabled.load(std::memory_order_acquire) && pending != noMapping)
    {
        midiCcToParameter[static_cast<std::size_t>(cc)].store(pending, std::memory_order_release);
        midiLearnEnabled.store(false, std::memory_order_release);
        pendingLearnParameter.store(noMapping, std::memory_order_release);
        return;
    }

    if (! midiControlEnabled.load(std::memory_order_acquire))
        return;

    const auto index = midiCcToParameter[static_cast<std::size_t>(cc)].load(std::memory_order_acquire);
    if (index == noMapping)
        return;

    if (auto* parameter = parameters.getParameter(parameterIdForIndex(index)))
    {
        const auto normalized = std::clamp(static_cast<float>(value) / 127.0f, 0.0f, 1.0f);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(normalized);
        parameter->endChangeGesture();
    }
}

int NebulaJEsserAudioProcessor::parameterIndexForId(const juce::String& parameterId) const
{
    const auto& infos = nebula::state::automatableParameters();
    for (std::size_t index = 0; index < infos.size(); ++index)
    {
        const auto id = juce::String::fromUTF8(infos[index].id.data(), static_cast<int>(infos[index].id.size()));
        if (id == parameterId)
            return static_cast<int>(index);
    }
    return noMapping;
}

juce::String NebulaJEsserAudioProcessor::parameterIdForIndex(int index) const
{
    const auto& infos = nebula::state::automatableParameters();
    if (index < 0 || static_cast<std::size_t>(index) >= infos.size())
        return {};

    const auto id = infos[static_cast<std::size_t>(index)].id;
    return juce::String::fromUTF8(id.data(), static_cast<int>(id.size()));
}

void NebulaJEsserAudioProcessor::setEditorSize(int width, int height) noexcept
{
    editorWidth.store(std::clamp(width, 400, 8192), std::memory_order_release);
    editorHeight.store(std::clamp(height, 300, 8192), std::memory_order_release);
}

std::pair<int, int> NebulaJEsserAudioProcessor::editorSize() const noexcept
{
    return {
        editorWidth.load(std::memory_order_acquire),
        editorHeight.load(std::memory_order_acquire)
    };
}

juce::ValueTree NebulaJEsserAudioProcessor::captureParameterState() const
{
    juce::ValueTree tree { "PARAMETER_SNAPSHOT" };
    for (const auto& info : nebula::state::automatableParameters())
    {
        const auto id = juce::String::fromUTF8(info.id.data(), static_cast<int>(info.id.size()));
        if (const auto* value = parameters.getRawParameterValue(id); value != nullptr)
            tree.setProperty(id, static_cast<double>(value->load()), nullptr);
    }
    return tree;
}

void NebulaJEsserAudioProcessor::applyParameterState(const juce::ValueTree& tree)
{
    for (const auto& info : nebula::state::automatableParameters())
    {
        const auto id = juce::String::fromUTF8(info.id.data(), static_cast<int>(info.id.size()));
        if (! tree.hasProperty(id))
            continue;

        if (auto* parameter = parameters.getParameter(id))
        {
            const auto rawValue = static_cast<float>(static_cast<double>(tree[id]));
            const auto normalized = parameter->convertTo0to1(rawValue);
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(normalized);
            parameter->endChangeGesture();
        }
    }
}

void NebulaJEsserAudioProcessor::savePreset(const juce::String& name)
{
    const auto trimmed = name.trim();
    if (trimmed.isEmpty())
        return;

    std::lock_guard lock { presetMutex };
    for (int index = presets.getNumChildren(); --index >= 0;)
    {
        if (presets.getChild(index).getProperty("name").toString() == trimmed)
            presets.removeChild(index, nullptr);
    }

    auto preset = captureParameterState();
    preset.setProperty("name", trimmed, nullptr);
    presets.addChild(preset, -1, nullptr);
}

void NebulaJEsserAudioProcessor::loadPreset(int index)
{
    std::lock_guard lock { presetMutex };
    if (index < 0 || index >= presets.getNumChildren())
        return;
    applyParameterState(presets.getChild(index));
}

juce::StringArray NebulaJEsserAudioProcessor::presetNames() const
{
    std::lock_guard lock { presetMutex };
    juce::StringArray names;
    for (int index = 0; index < presets.getNumChildren(); ++index)
        names.add(presets.getChild(index).getProperty("name").toString());
    return names;
}

void NebulaJEsserAudioProcessor::ensureABInitialized()
{
    if (! stateA.isValid() || stateA.getNumProperties() == 0)
    {
        const auto snapshot = captureParameterState();
        stateA = juce::ValueTree { "STATE_A" };
        stateA.copyPropertiesFrom(snapshot, nullptr);
    }
    if (! stateB.isValid() || stateB.getNumProperties() == 0)
    {
        const auto snapshot = captureParameterState();
        stateB = juce::ValueTree { "STATE_B" };
        stateB.copyPropertiesFrom(snapshot, nullptr);
    }
}

void NebulaJEsserAudioProcessor::switchAB()
{
    std::lock_guard lock { presetMutex };
    ensureABInitialized();
    const auto current = activeAB.load(std::memory_order_acquire);
    if (current == 0)
    {
        const auto snapshot = captureParameterState();
        stateA = juce::ValueTree { "STATE_A" };
        stateA.copyPropertiesFrom(snapshot, nullptr);
        applyParameterState(stateB);
        activeAB.store(1, std::memory_order_release);
    }
    else
    {
        const auto snapshot = captureParameterState();
        stateB = juce::ValueTree { "STATE_B" };
        stateB.copyPropertiesFrom(snapshot, nullptr);
        applyParameterState(stateA);
        activeAB.store(0, std::memory_order_release);
    }
}

void NebulaJEsserAudioProcessor::setMidiLearnEnabled(bool shouldLearn) noexcept
{
    midiLearnEnabled.store(shouldLearn, std::memory_order_release);
    if (! shouldLearn)
        pendingLearnParameter.store(noMapping, std::memory_order_release);
}

void NebulaJEsserAudioProcessor::selectMidiLearnTarget(const juce::String& parameterId)
{
    const auto index = parameterIndexForId(parameterId);
    if (index != noMapping)
        pendingLearnParameter.store(index, std::memory_order_release);
}

void NebulaJEsserAudioProcessor::clearMidiMapping(int cc) noexcept
{
    if (cc >= 0 && cc < midiCcCount)
        midiCcToParameter[static_cast<std::size_t>(cc)].store(noMapping, std::memory_order_release);
}

void NebulaJEsserAudioProcessor::clearAllMidiMappings() noexcept
{
    for (auto& mapping : midiCcToParameter)
        mapping.store(noMapping, std::memory_order_release);
}

void NebulaJEsserAudioProcessor::saveMidiRollbackPoint()
{
    for (std::size_t index = 0; index < midiCcToParameter.size(); ++index)
    {
        savedMidiCcToParameter[index].store(
            midiCcToParameter[index].load(std::memory_order_acquire),
            std::memory_order_release);
    }
}

void NebulaJEsserAudioProcessor::rollBackMidiMappings() noexcept
{
    for (std::size_t index = 0; index < midiCcToParameter.size(); ++index)
    {
        midiCcToParameter[index].store(
            savedMidiCcToParameter[index].load(std::memory_order_acquire),
            std::memory_order_release);
    }
}

juce::StringArray NebulaJEsserAudioProcessor::midiMappingDescriptions() const
{
    juce::StringArray descriptions;
    for (std::size_t cc = 0; cc < midiCcToParameter.size(); ++cc)
    {
        const auto index = midiCcToParameter[cc].load(std::memory_order_acquire);
        if (index != noMapping)
        {
            const auto parameterId = parameterIdForIndex(index);
            descriptions.add("CC " + juce::String(static_cast<int>(cc)) + " -> " + nebula::state::parameterName(parameterId.toRawUTF8()));
        }
    }
    return descriptions;
}

juce::ValueTree NebulaJEsserAudioProcessor::midiMappingsToTree() const
{
    juce::ValueTree tree { "MIDI_MAPPINGS" };
    tree.setProperty("enabled", midiControlEnabled.load(std::memory_order_acquire), nullptr);
    for (std::size_t cc = 0; cc < midiCcToParameter.size(); ++cc)
    {
        const auto index = midiCcToParameter[cc].load(std::memory_order_acquire);
        if (index == noMapping)
            continue;

        juce::ValueTree binding { "BINDING" };
        binding.setProperty("cc", static_cast<int>(cc), nullptr);
        binding.setProperty("parameter", parameterIdForIndex(index), nullptr);
        tree.addChild(binding, -1, nullptr);
    }
    return tree;
}

void NebulaJEsserAudioProcessor::midiMappingsFromTree(const juce::ValueTree& tree)
{
    clearAllMidiMappings();
    if (! tree.isValid())
        return;

    midiControlEnabled.store(static_cast<bool>(tree.getProperty("enabled", true)), std::memory_order_release);
    for (int childIndex = 0; childIndex < tree.getNumChildren(); ++childIndex)
    {
        const auto child = tree.getChild(childIndex);
        const auto cc = static_cast<int>(child.getProperty("cc", noMapping));
        const auto parameter = child.getProperty("parameter").toString();
        const auto index = parameterIndexForId(parameter);
        if (cc >= 0 && cc < midiCcCount && index != noMapping)
            midiCcToParameter[static_cast<std::size_t>(cc)].store(index, std::memory_order_release);
    }
    saveMidiRollbackPoint();
}

void NebulaJEsserAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto tree = parameters.copyState();
    while (tree.getChildWithName("PRESETS").isValid())
        tree.removeChild(tree.getChildWithName("PRESETS"), nullptr);
    while (tree.getChildWithName("STATE_A").isValid())
        tree.removeChild(tree.getChildWithName("STATE_A"), nullptr);
    while (tree.getChildWithName("STATE_B").isValid())
        tree.removeChild(tree.getChildWithName("STATE_B"), nullptr);
    while (tree.getChildWithName("MIDI_MAPPINGS").isValid())
        tree.removeChild(tree.getChildWithName("MIDI_MAPPINGS"), nullptr);

    tree.setProperty("editorWidth", editorWidth.load(std::memory_order_acquire), nullptr);
    tree.setProperty("editorHeight", editorHeight.load(std::memory_order_acquire), nullptr);
    tree.setProperty("activeAB", activeAB.load(std::memory_order_acquire), nullptr);

    {
        std::lock_guard lock { presetMutex };
        tree.addChild(presets.createCopy(), -1, nullptr);
        tree.addChild(stateA.createCopy(), -1, nullptr);
        tree.addChild(stateB.createCopy(), -1, nullptr);
    }

    tree.addChild(midiMappingsToTree(), -1, nullptr);

    if (auto xml = tree.createXml(); xml != nullptr)
        copyXmlToBinary(*xml, destData);
}

void NebulaJEsserAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes); xml != nullptr)
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (! tree.isValid())
            return;

        editorWidth.store(static_cast<int>(tree.getProperty("editorWidth", 1180)), std::memory_order_release);
        editorHeight.store(static_cast<int>(tree.getProperty("editorHeight", 720)), std::memory_order_release);
        activeAB.store(static_cast<int>(tree.getProperty("activeAB", 0)), std::memory_order_release);

        {
            std::lock_guard lock { presetMutex };
            presets = tree.getChildWithName("PRESETS");
            if (! presets.isValid())
                presets = juce::ValueTree { "PRESETS" };

            stateA = tree.getChildWithName("STATE_A");
            stateB = tree.getChildWithName("STATE_B");
            ensureABInitialized();
        }

        midiMappingsFromTree(tree.getChildWithName("MIDI_MAPPINGS"));

        auto parameterTree = tree.createCopy();
        while (parameterTree.getChildWithName("PRESETS").isValid())
            parameterTree.removeChild(parameterTree.getChildWithName("PRESETS"), nullptr);
        while (parameterTree.getChildWithName("STATE_A").isValid())
            parameterTree.removeChild(parameterTree.getChildWithName("STATE_A"), nullptr);
        while (parameterTree.getChildWithName("STATE_B").isValid())
            parameterTree.removeChild(parameterTree.getChildWithName("STATE_B"), nullptr);
        while (parameterTree.getChildWithName("MIDI_MAPPINGS").isValid())
            parameterTree.removeChild(parameterTree.getChildWithName("MIDI_MAPPINGS"), nullptr);

        parameters.replaceState(parameterTree);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NebulaJEsserAudioProcessor();
}
