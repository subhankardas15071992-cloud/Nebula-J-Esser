#include "NebulaJEsserEditor.h"

#include "state/Parameters.h"

#include <algorithm>
#include <array>

namespace
{
[[nodiscard]] int scaled(int value, int height)
{
    return juce::jlimit(value - 6, value + 10, static_cast<int>(static_cast<float>(height) * 0.075f));
}
} // namespace

NebulaJEsserAudioProcessorEditor::NebulaJEsserAudioProcessorEditor(NebulaJEsserAudioProcessor& processor)
    : AudioProcessorEditor(&processor),
      audioProcessor(processor),
      spectrum(processor, processor.state()),
      detectionMeter(processor, processor.state(), nebula::state::ids::tkeoSharp.data(), "DETECTION", false),
      reductionMeter(processor, processor.state(), nebula::state::ids::annihilation.data(), "ANNIHILATION", true),
      tkeoKnob(processor, processor.state(), nebula::state::ids::tkeoSharp.data(), "TKEO SHARP"),
      annihilationKnob(processor, processor.state(), nebula::state::ids::annihilation.data(), "ANNIHILATION"),
      mixKnob(processor, processor.state(), nebula::state::ids::mix.data(), "MIX"),
      modeCombo(processor, processor.state(), nebula::state::ids::mode.data(), "MODE"),
      profileCombo(processor, processor.state(), nebula::state::ids::profile.data(), "SOURCE"),
      rangeCombo(processor, processor.state(), nebula::state::ids::range.data(), "RANGE"),
      minFrequencyKnob(processor, processor.state(), nebula::state::ids::minFrequency.data(), "MIN FREQ"),
      maxFrequencyKnob(processor, processor.state(), nebula::state::ids::maxFrequency.data(), "MAX FREQ"),
      cutWidthKnob(processor, processor.state(), nebula::state::ids::cutWidth.data(), "CUT WIDTH"),
      cutDepthKnob(processor, processor.state(), nebula::state::ids::cutDepth.data(), "CUT DEPTH"),
      cutSlopeKnob(processor, processor.state(), nebula::state::ids::cutSlope.data(), "CUT SLOPE"),
      filterCombo(processor, processor.state(), nebula::state::ids::filter.data(), "FILTER"),
      filterSoloToggle(processor, processor.state(), nebula::state::ids::filterSolo.data(), "FILTER SOLO"),
      triggerHearToggle(processor, processor.state(), nebula::state::ids::triggerHear.data(), "TRIGGER HEAR"),
      sideChainCombo(processor, processor.state(), nebula::state::ids::sideChain.data(), "SIDE CHAIN"),
      stereoLinkKnob(processor, processor.state(), nebula::state::ids::stereoLink.data(), "STEREO LINK"),
      stereoModeCombo(processor, processor.state(), nebula::state::ids::stereoMode.data(), "LINK MODE"),
      lookaheadToggle(processor, processor.state(), nebula::state::ids::lookaheadEnabled.data(), "LOOKAHEAD"),
      lookaheadKnob(processor, processor.state(), nebula::state::ids::lookaheadMs.data(), "LOOKAHEAD MS"),
      inputLevelKnob(processor, processor.state(), nebula::state::ids::inputLevel.data(), "INPUT LEVEL"),
      inputPanKnob(processor, processor.state(), nebula::state::ids::inputPan.data(), "INPUT PAN"),
      outputLevelKnob(processor, processor.state(), nebula::state::ids::outputLevel.data(), "OUTPUT LEVEL"),
      outputPanKnob(processor, processor.state(), nebula::state::ids::outputPan.data(), "OUTPUT PAN")
{
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);

    titleLabel.setText("NEBULA J-ESSER", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, nebula::ui::SciFiLookAndFeel::text());
    titleLabel.setFont(juce::Font { juce::FontOptions(23.0f, juce::Font::bold) });

    companyLabel.setText("Nebula Audio", juce::dontSendNotification);
    companyLabel.setJustificationType(juce::Justification::centredLeft);
    companyLabel.setColour(juce::Label::textColourId, nebula::ui::SciFiLookAndFeel::mutedText());
    companyLabel.setFont(juce::Font { juce::FontOptions(12.5f) });

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(companyLabel);
    addAndMakeVisible(presetBox);
    addAndMakeVisible(presetSaveButton);
    addAndMakeVisible(undoButton);
    addAndMakeVisible(redoButton);
    addAndMakeVisible(abButton);
    addAndMakeVisible(midiButton);
    addAndMakeVisible(bypassButton);
    addAndMakeVisible(spectrum);
    addAndMakeVisible(tabs);

    coreViewport.setViewedComponent(&corePage, false);
    shapeViewport.setViewedComponent(&shapePage, false);
    routingViewport.setViewedComponent(&routingPage, false);
    coreViewport.setScrollBarsShown(true, false);
    shapeViewport.setScrollBarsShown(true, false);
    routingViewport.setScrollBarsShown(true, false);
    tabs.addTab("CORE", nebula::ui::SciFiLookAndFeel::panel(), &coreViewport, false);
    tabs.addTab("SHAPE", nebula::ui::SciFiLookAndFeel::panel(), &shapeViewport, false);
    tabs.addTab("ROUTING", nebula::ui::SciFiLookAndFeel::panel(), &routingViewport, false);
    tabs.setTabBarDepth(30);

    for (auto* component : std::array<juce::Component*, 8> { &detectionMeter, &reductionMeter, &tkeoKnob, &annihilationKnob, &mixKnob, &modeCombo, &profileCombo, &rangeCombo })
        corePage.addAndMakeVisible(component);

    for (auto* component : std::array<juce::Component*, 8> { &minFrequencyKnob, &maxFrequencyKnob, &cutWidthKnob, &cutDepthKnob, &cutSlopeKnob, &filterCombo, &filterSoloToggle, &triggerHearToggle })
        shapePage.addAndMakeVisible(component);

    for (auto* component : std::array<juce::Component*, 9> { &sideChainCombo, &stereoLinkKnob, &stereoModeCombo, &lookaheadToggle, &lookaheadKnob, &inputLevelKnob, &inputPanKnob, &outputLevelKnob, &outputPanKnob })
        routingPage.addAndMakeVisible(component);

    presetBox.setTextWhenNothingSelected("Presets");
    presetBox.onChange = [this]
    {
        const auto index = presetBox.getSelectedId() - 1;
        if (index >= 0)
            audioProcessor.loadPreset(index);
    };

    presetSaveButton.onClick = [this]
    {
        nebula::ui::showPresetNameBox(presetSaveButton, [this] (juce::String name)
        {
            audioProcessor.savePreset(name);
            refreshPresetList();
        });
    };

    undoButton.onClick = [this] { audioProcessor.undo().undo(); };
    redoButton.onClick = [this] { audioProcessor.undo().redo(); };
    abButton.onClick = [this]
    {
        audioProcessor.switchAB();
        abButton.setToggleState(audioProcessor.isStateA(), juce::dontSendNotification);
    };

    midiButton.onClick = [this]
    {
        audioProcessor.setMidiLearnEnabled(! audioProcessor.isMidiLearnEnabled());
        midiButton.setToggleState(audioProcessor.isMidiLearnEnabled(), juce::dontSendNotification);
    };
    midiButton.onRightClick = [this] { showMidiMenu(); };

    bypassButton.setClickingTogglesState(true);
    bypassButton.onTouch = [this] { audioProcessor.selectMidiLearnTarget(nebula::state::ids::bypass.data()); };
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.state(), nebula::state::ids::bypass.data(), bypassButton);

    refreshPresetList();
    const auto [width, height] = audioProcessor.editorSize();
    const auto* primaryDisplay = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    const auto displayArea = primaryDisplay != nullptr ? primaryDisplay->userArea : juce::Rectangle<int>(0, 0, 8192, 8192);
    const auto boundedWidth = juce::jlimit(400, std::max(400, displayArea.getWidth()), width);
    const auto boundedHeight = juce::jlimit(300, std::max(300, displayArea.getHeight()), height);
    setResizable(true, true);
    setResizeLimits(400, 300, std::max(400, displayArea.getWidth()), std::max(300, displayArea.getHeight()));
    setSize(boundedWidth, boundedHeight);
    startTimerHz(30);
}

NebulaJEsserAudioProcessorEditor::~NebulaJEsserAudioProcessorEditor()
{
    audioProcessor.setEditorSize(getWidth(), getHeight());
    setLookAndFeel(nullptr);
}

void NebulaJEsserAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(nebula::ui::SciFiLookAndFeel::voidBlack());
    g.fillRect(bounds);

    juce::ColourGradient glow(nebula::ui::SciFiLookAndFeel::cyan().withAlpha(0.20f), bounds.getX(), bounds.getY(),
                              nebula::ui::SciFiLookAndFeel::magenta().withAlpha(0.11f), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill(glow);
    g.fillRect(bounds);

    g.setColour(nebula::ui::SciFiLookAndFeel::grid().withAlpha(0.20f));
    const auto step = std::max(26, getWidth() / 34);
    for (int x = 0; x < getWidth(); x += step)
        g.drawVerticalLine(x, 0.0f, static_cast<float>(getHeight()));
    for (int y = 0; y < getHeight(); y += step)
        g.drawHorizontalLine(y, 0.0f, static_cast<float>(getWidth()));
}

void NebulaJEsserAudioProcessorEditor::resized()
{
    audioProcessor.setEditorSize(getWidth(), getHeight());
    auto area = getLocalBounds().reduced(10);
    const auto topHeight = getWidth() < 700 ? 72 : scaled(52, getHeight());
    layoutTopBar(area.removeFromTop(topHeight));
    area.removeFromTop(8);

    const auto analyzerHeight = juce::jlimit(86, 230, static_cast<int>(static_cast<float>(getHeight()) * 0.29f));
    spectrum.setBounds(area.removeFromTop(analyzerHeight));
    area.removeFromTop(8);

    tabs.setBounds(area);
    layoutCorePage();
    layoutShapePage();
    layoutRoutingPage();
}

void NebulaJEsserAudioProcessorEditor::layoutTopBar(juce::Rectangle<int> area)
{
    if (area.getWidth() < 700)
    {
        auto row = area.removeFromTop(36);
        const auto buttonWidth = 32;
        bypassButton.setBounds(row.removeFromRight(buttonWidth));
        row.removeFromRight(4);
        midiButton.setBounds(row.removeFromRight(buttonWidth));
        row.removeFromRight(4);
        abButton.setBounds(row.removeFromRight(buttonWidth));
        row.removeFromRight(4);
        redoButton.setBounds(row.removeFromRight(buttonWidth));
        row.removeFromRight(4);
        undoButton.setBounds(row.removeFromRight(buttonWidth));
        titleLabel.setBounds(row.removeFromTop(20));
        companyLabel.setBounds(row);

        area.removeFromTop(4);
        presetSaveButton.setBounds(area.removeFromRight(38));
        area.removeFromRight(6);
        presetBox.setBounds(area);
        return;
    }

    const auto buttonWidth = juce::jlimit(34, 44, area.getHeight() - 8);
    bypassButton.setBounds(area.removeFromRight(buttonWidth));
    area.removeFromRight(5);
    midiButton.setBounds(area.removeFromRight(buttonWidth + 16));
    area.removeFromRight(5);
    abButton.setBounds(area.removeFromRight(buttonWidth + 6));
    area.removeFromRight(5);
    redoButton.setBounds(area.removeFromRight(buttonWidth));
    area.removeFromRight(5);
    undoButton.setBounds(area.removeFromRight(buttonWidth));
    area.removeFromRight(8);
    presetSaveButton.setBounds(area.removeFromRight(buttonWidth));
    area.removeFromRight(6);
    presetBox.setBounds(area.removeFromRight(std::min(280, area.getWidth() / 3)).reduced(0, 4));
    area.removeFromRight(10);

    const auto titleWidth = std::min(360, area.getWidth());
    auto titleArea = area.removeFromLeft(titleWidth);
    titleLabel.setBounds(titleArea.removeFromTop(titleArea.getHeight() / 2 + 4));
    companyLabel.setBounds(titleArea);
}

void NebulaJEsserAudioProcessorEditor::layoutGrid(const std::vector<juce::Component*>& controls, juce::Rectangle<int> area, int preferredCellHeight)
{
    if (controls.empty())
        return;

    area = area.reduced(4);
    const auto columns = juce::jlimit(1, 4, area.getWidth() / 140);
    const auto rows = static_cast<int>((controls.size() + static_cast<std::size_t>(columns) - 1U) / static_cast<std::size_t>(columns));
    const auto cellHeight = std::max(54, std::min(preferredCellHeight, area.getHeight() / std::max(1, rows)));
    const auto cellWidth = area.getWidth() / columns;

    for (std::size_t index = 0; index < controls.size(); ++index)
    {
        const auto column = static_cast<int>(index % static_cast<std::size_t>(columns));
        const auto row = static_cast<int>(index / static_cast<std::size_t>(columns));
        juce::Rectangle<int> cell(area.getX() + column * cellWidth,
                                  area.getY() + row * cellHeight,
                                  column == columns - 1 ? area.getRight() - (area.getX() + column * cellWidth) : cellWidth,
                                  cellHeight);
        controls[index]->setBounds(cell.reduced(5));
    }
}

void NebulaJEsserAudioProcessorEditor::layoutCorePage()
{
    const auto width = std::max(1, coreViewport.getMaximumVisibleWidth());
    const auto visibleHeight = std::max(1, coreViewport.getMaximumVisibleHeight());
    const auto columns = juce::jlimit(1, 4, width / 140);
    const auto controlRows = 2 + (columns <= 2 ? 1 : 0);
    const auto requiredHeight = width < 620 ? 126 + controlRows * 112 : 300;
    corePage.setSize(width, std::max(visibleHeight, requiredHeight));

    auto area = corePage.getLocalBounds().reduced(8);
    if (area.getWidth() < 620)
    {
        auto meters = area.removeFromTop(std::max(78, area.getHeight() / 3));
        detectionMeter.setBounds(meters.removeFromLeft(meters.getWidth() / 2).reduced(3));
        reductionMeter.setBounds(meters.reduced(3));
        layoutGrid({ &modeCombo, &profileCombo, &rangeCombo, &tkeoKnob, &annihilationKnob, &mixKnob }, area, 104);
        return;
    }

    auto meterArea = area.removeFromLeft(std::min(360, std::max(250, area.getWidth() / 3)));
    detectionMeter.setBounds(meterArea.removeFromTop(meterArea.getHeight() / 2).reduced(4));
    reductionMeter.setBounds(meterArea.reduced(4));
    layoutGrid({ &modeCombo, &profileCombo, &rangeCombo, &tkeoKnob, &annihilationKnob, &mixKnob }, area, 136);
}

void NebulaJEsserAudioProcessorEditor::layoutShapePage()
{
    const auto width = std::max(1, shapeViewport.getMaximumVisibleWidth());
    const auto visibleHeight = std::max(1, shapeViewport.getMaximumVisibleHeight());
    const auto columns = juce::jlimit(1, 4, width / 140);
    const auto rows = static_cast<int>((8 + columns - 1) / columns);
    shapePage.setSize(width, std::max(visibleHeight, rows * 126 + 18));

    auto area = shapePage.getLocalBounds().reduced(8);
    layoutGrid({ &filterCombo, &filterSoloToggle, &triggerHearToggle, &minFrequencyKnob, &maxFrequencyKnob, &cutWidthKnob, &cutDepthKnob, &cutSlopeKnob }, area, 132);
}

void NebulaJEsserAudioProcessorEditor::layoutRoutingPage()
{
    const auto width = std::max(1, routingViewport.getMaximumVisibleWidth());
    const auto visibleHeight = std::max(1, routingViewport.getMaximumVisibleHeight());
    const auto columns = juce::jlimit(1, 4, width / 140);
    const auto rows = static_cast<int>((9 + columns - 1) / columns);
    routingPage.setSize(width, std::max(visibleHeight, rows * 122 + 18));

    auto area = routingPage.getLocalBounds().reduced(8);
    layoutGrid({ &sideChainCombo, &stereoModeCombo, &lookaheadToggle, &stereoLinkKnob, &lookaheadKnob, &inputLevelKnob, &inputPanKnob, &outputLevelKnob, &outputPanKnob }, area, 124);
}

void NebulaJEsserAudioProcessorEditor::timerCallback()
{
    const auto& meters = audioProcessor.meterFrame();
    detectionMeter.setMeterValues(meters.detection.load(), meters.detectionMax.load());
    reductionMeter.setMeterValues(meters.reduction.load(), meters.reductionMax.load());
    spectrum.refresh();
    midiButton.setToggleState(audioProcessor.isMidiLearnEnabled(), juce::dontSendNotification);
    abButton.setToggleState(audioProcessor.isStateA(), juce::dontSendNotification);

    const auto count = audioProcessor.presetNames().size();
    if (count != knownPresetCount)
        refreshPresetList();
}

void NebulaJEsserAudioProcessorEditor::refreshPresetList()
{
    const auto names = audioProcessor.presetNames();
    knownPresetCount = names.size();
    presetBox.clear(juce::dontSendNotification);
    for (int index = 0; index < names.size(); ++index)
        presetBox.addItem(names[index], index + 1);
}

void NebulaJEsserAudioProcessorEditor::showMidiMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "MIDI On/Off", true, audioProcessor.isMidiControlEnabled());

    juce::PopupMenu clean;
    clean.addItem(4, "Clear All", true, false);
    const auto descriptions = audioProcessor.midiMappingDescriptions();
    for (int index = 0; index < descriptions.size(); ++index)
        clean.addItem(100 + index, descriptions[index], true, false);
    menu.addSubMenu("Clean Up", clean);
    menu.addItem(2, "Roll Back");
    menu.addItem(3, "Save");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&midiButton),
        [this, descriptions] (int result)
        {
            if (result == 1)
                audioProcessor.setMidiControlEnabled(! audioProcessor.isMidiControlEnabled());
            else if (result == 2)
                audioProcessor.rollBackMidiMappings();
            else if (result == 3)
                audioProcessor.saveMidiRollbackPoint();
            else if (result == 4)
                audioProcessor.clearAllMidiMappings();
            else if (result >= 100)
            {
                const auto description = descriptions[result - 100];
                const auto ccText = description.fromFirstOccurrenceOf("CC ", false, false).upToFirstOccurrenceOf(" ", false, false);
                audioProcessor.clearMidiMapping(ccText.getIntValue());
            }
        });
}
