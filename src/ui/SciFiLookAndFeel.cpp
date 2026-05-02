#include "ui/SciFiLookAndFeel.h"

#include <cmath>

namespace nebula::ui
{
namespace
{
class SliderValueLabel final : public juce::Label
{
public:
    explicit SliderValueLabel(juce::Slider& ownerSlider)
        : slider(ownerSlider)
    {
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
        {
            slider.showTextBox();
            return;
        }

        juce::Label::mouseDown(event);
    }

private:
    juce::Slider& slider;
};
} // namespace

SciFiLookAndFeel::SciFiLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, text());
    setColour(juce::Slider::textBoxBackgroundColourId, panelRaised());
    setColour(juce::Slider::textBoxOutlineColourId, grid());
    setColour(juce::Label::textColourId, text());
    setColour(juce::ComboBox::textColourId, text());
    setColour(juce::ComboBox::backgroundColourId, panelRaised());
    setColour(juce::ComboBox::outlineColourId, grid());
    setColour(juce::PopupMenu::backgroundColourId, panel());
    setColour(juce::PopupMenu::textColourId, text());
    setColour(juce::TextButton::textColourOffId, text());
    setColour(juce::TextButton::textColourOnId, voidBlack());
    setColour(juce::CaretComponent::caretColourId, cyan());
}

void SciFiLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                        int x,
                                        int y,
                                        int width,
                                        int height,
                                        float sliderPosProportional,
                                        float rotaryStartAngle,
                                        float rotaryEndAngle,
                                        juce::Slider& slider)
{
    juce::ignoreUnused(slider);
    const auto bounds = juce::Rectangle<float> { static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height) }
        .reduced(6.0f);
    const auto diameter = std::min(bounds.getWidth(), bounds.getHeight());
    const auto area = bounds.withSizeKeepingCentre(diameter, diameter);
    const auto radius = area.getWidth() * 0.5f;
    const auto centre = area.getCentre();
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    g.setColour(panelRaised());
    g.fillEllipse(area);

    g.setColour(grid().withAlpha(0.55f));
    g.drawEllipse(area.reduced(1.5f), 1.4f);

    juce::Path backgroundArc;
    backgroundArc.addCentredArc(centre.x, centre.y, radius - 4.0f, radius - 4.0f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(grid().withAlpha(0.75f));
    g.strokePath(backgroundArc, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path activeArc;
    activeArc.addCentredArc(centre.x, centre.y, radius - 4.0f, radius - 4.0f, 0.0f, rotaryStartAngle, angle, true);
    juce::ColourGradient gradient(cyan(), area.getX(), area.getY(), magenta(), area.getRight(), area.getBottom(), false);
    g.setGradientFill(gradient);
    g.strokePath(activeArc, juce::PathStrokeType(5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto indicatorLength = radius * 0.54f;
    const auto indicatorStart = radius * 0.17f;
    const auto dx = std::cos(angle - juce::MathConstants<float>::halfPi);
    const auto dy = std::sin(angle - juce::MathConstants<float>::halfPi);
    g.setColour(text());
    g.drawLine(centre.x + dx * indicatorStart,
               centre.y + dy * indicatorStart,
               centre.x + dx * indicatorLength,
               centre.y + dy * indicatorLength,
               2.0f);

    g.setColour(voidBlack().withAlpha(0.85f));
    g.fillEllipse(area.reduced(radius * 0.58f));
}

void SciFiLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                        int x,
                                        int y,
                                        int width,
                                        int height,
                                        float sliderPos,
                                        float minSliderPos,
                                        float maxSliderPos,
                                        const juce::Slider::SliderStyle style,
                                        juce::Slider& slider)
{
    juce::ignoreUnused(minSliderPos, maxSliderPos, slider);
    const auto bounds = juce::Rectangle<float> { static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height) }
        .reduced(4.0f);

    g.setColour(panelRaised());
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(grid());
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

    if (style == juce::Slider::LinearVertical)
    {
        const auto filled = bounds.withY(sliderPos).withBottom(bounds.getBottom());
        g.setColour(cyan().withAlpha(0.8f));
        g.fillRoundedRectangle(filled, 5.0f);
        g.setColour(text());
        g.fillRect(juce::Rectangle<float>(bounds.getX(), sliderPos - 1.0f, bounds.getWidth(), 2.0f));
    }
    else
    {
        const auto filled = bounds.withRight(sliderPos);
        g.setColour(cyan().withAlpha(0.8f));
        g.fillRoundedRectangle(filled, 5.0f);
        g.setColour(text());
        g.fillRect(juce::Rectangle<float>(sliderPos - 1.0f, bounds.getY(), 2.0f, bounds.getHeight()));
    }
}

void SciFiLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                            juce::Button& button,
                                            const juce::Colour& backgroundColour,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const auto active = button.getToggleState();
    auto colour = active ? cyan() : backgroundColour;
    if (shouldDrawButtonAsHighlighted)
        colour = colour.brighter(0.15f);
    if (shouldDrawButtonAsDown)
        colour = colour.darker(0.18f);

    g.setColour(colour.withAlpha(active ? 0.92f : 0.72f));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour((active ? cyan() : grid()).withAlpha(0.95f));
    g.drawRoundedRectangle(bounds, 5.0f, 1.2f);
}

void SciFiLookAndFeel::drawToggleButton(juce::Graphics& g,
                                        juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool shouldDrawButtonAsDown)
{
    drawButtonBackground(g, button, panelRaised(), shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    g.setColour(button.getToggleState() ? voidBlack() : text());
    g.setFont(juce::Font { juce::FontOptions(12.5f, juce::Font::plain) });
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(6, 1), juce::Justification::centred, 1);
}

void SciFiLookAndFeel::drawComboBox(juce::Graphics& g,
                                    int width,
                                    int height,
                                    bool isButtonDown,
                                    int buttonX,
                                    int buttonY,
                                    int buttonW,
                                    int buttonH,
                                    juce::ComboBox& box)
{
    juce::ignoreUnused(isButtonDown, buttonX, buttonY, buttonW, buttonH, box);
    const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);
    g.setColour(panelRaised());
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(grid());
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

    juce::Path arrow;
    const auto cx = bounds.getRight() - 14.0f;
    const auto cy = bounds.getCentreY();
    arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 4.0f);
    g.setColour(cyan());
    g.fillPath(arrow);
}

void SciFiLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.setColour(label.findColour(juce::Label::backgroundColourId));
    if (! label.findColour(juce::Label::backgroundColourId).isTransparent())
        g.fillRoundedRectangle(label.getLocalBounds().toFloat(), 4.0f);

    g.setColour(label.findColour(juce::Label::textColourId));
    g.setFont(label.getFont());
    g.drawFittedText(label.getText(), label.getLocalBounds().reduced(2), label.getJustificationType(), 1);
}

juce::Label* SciFiLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = new SliderValueLabel(slider);
    label->setJustificationType(juce::Justification::centred);
    label->setColour(juce::Label::backgroundColourId, panelRaised());
    label->setColour(juce::Label::outlineColourId, grid());
    label->setColour(juce::Label::textColourId, text());
    label->setColour(juce::TextEditor::textColourId, text());
    label->setColour(juce::TextEditor::backgroundColourId, panelRaised());
    label->setColour(juce::TextEditor::highlightColourId, cyan().withAlpha(0.2f));
    label->setColour(juce::TextEditor::highlightedTextColourId, text());
    label->setColour(juce::CaretComponent::caretColourId, cyan());
    label->setFont(juce::Font { juce::FontOptions(12.0f) });
    label->setEditable(true, true, false);
    return label;
}

void SciFiLookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& textEditor)
{
    juce::ignoreUnused(textEditor);
    g.setColour(panelRaised());
    g.fillRoundedRectangle(juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)), 5.0f);
}

void SciFiLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& textEditor)
{
    juce::ignoreUnused(textEditor);
    g.setColour(cyan());
    g.drawRoundedRectangle(juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f), 5.0f, 1.0f);
}
} // namespace nebula::ui
