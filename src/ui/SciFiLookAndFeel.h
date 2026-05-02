#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace nebula::ui
{
class SciFiLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SciFiLookAndFeel();

    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawLinearSlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics& g,
                      int width,
                      int height,
                      bool isButtonDown,
                      int buttonX,
                      int buttonY,
                      int buttonW,
                      int buttonH,
                      juce::ComboBox& box) override;

    void drawLabel(juce::Graphics& g, juce::Label& label) override;
    juce::Label* createSliderTextBox(juce::Slider& slider) override;
    void fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override;
    void drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override;

    static juce::Colour voidBlack() noexcept { return juce::Colour::fromRGB(5, 8, 14); }
    static juce::Colour panel() noexcept { return juce::Colour::fromRGB(13, 20, 31); }
    static juce::Colour panelRaised() noexcept { return juce::Colour::fromRGB(20, 30, 45); }
    static juce::Colour grid() noexcept { return juce::Colour::fromRGB(35, 55, 78); }
    static juce::Colour cyan() noexcept { return juce::Colour::fromRGB(65, 245, 255); }
    static juce::Colour magenta() noexcept { return juce::Colour::fromRGB(255, 74, 214); }
    static juce::Colour amber() noexcept { return juce::Colour::fromRGB(255, 190, 74); }
    static juce::Colour text() noexcept { return juce::Colour::fromRGB(219, 238, 255); }
    static juce::Colour mutedText() noexcept { return juce::Colour::fromRGB(119, 150, 179); }
};
} // namespace nebula::ui
