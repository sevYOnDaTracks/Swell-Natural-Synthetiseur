#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Colour themes: dark brushed hardware with one accent colour. Default "Rubis" (red).
struct SwellPalette
{
    const char* name;
    juce::Colour accent, accentDeep, accentSoft;
};

namespace SwellTheme
{
inline const SwellPalette palettes[] = {
    { "Rubis",    juce::Colour (0xffe0342f), juce::Colour (0xff7a1512), juce::Colour (0xffff8a80) },
    { "Emeraude", juce::Colour (0xff22d38f), juce::Colour (0xff0d6448), juce::Colour (0xffa6f5d2) },
    { "Abysse",   juce::Colour (0xff3ea0ff), juce::Colour (0xff163f73), juce::Colour (0xffb5dcff) },
    { "Ambre",    juce::Colour (0xffff8c39), juce::Colour (0xff86410f), juce::Colour (0xffffd39d) },
};
inline constexpr int numThemes = 4;
inline int currentIndex = 0;
inline const SwellPalette& c() { return palettes[currentIndex]; }
inline juce::String displayName (int i)
{
    static const char* names[] = { "Rubis", "\xc3\x89meraude", "Abysse", "Ambre" };
    return juce::String::fromUTF8 (names[juce::jlimit (0, numThemes - 1, i)]);
}

// Neutral hardware colours
inline const juce::Colour bg       { 0xff0f0f10 };
inline const juce::Colour panelTop { 0xff1d1d20 };
inline const juce::Colour panelBot { 0xff141416 };
inline const juce::Colour well     { 0xff0b0b0c };
inline const juce::Colour edge     { 0xff2c2c30 };
inline const juce::Colour edgeHi   { 0xff3a3a3f };
inline const juce::Colour text     { 0xffeeeae6 };
inline const juce::Colour dim      { 0xffa8a4a0 };
inline const juce::Colour faint    { 0xff5f5c59 };
} // namespace SwellTheme

inline juce::Font swellFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

inline juce::Font serifFont (float size, bool bold = true)
{
    return juce::Font (juce::FontOptions ("Georgia", size, bold ? juce::Font::bold : juce::Font::plain));
}

inline juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

inline void drawStar (juce::Graphics& g, juce::Point<float> centre, float r, bool filled, juce::Colour col)
{
    juce::Path p;
    p.addStar (centre, 5, r * 0.45f, r, 0.0f);
    g.setColour (col);
    if (filled) g.fillPath (p);
    else g.strokePath (p, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// Hardware panel: vertical gradient, top highlight, dark edge.
inline void drawHardwarePanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 8.0f)
{
    using namespace SwellTheme;
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f).expanded (1.0f), radius + 1.0f);
    g.setGradientFill (juce::ColourGradient (panelTop, r.getX(), r.getY(), panelBot, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (edge);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + radius, r.getRight() - radius);
}

// Recessed area (lists, screens)
inline void drawWell (juce::Graphics& g, juce::Rectangle<float> r, float radius = 6.0f)
{
    g.setColour (SwellTheme::well);
    g.fillRoundedRectangle (r, radius);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX() + radius, r.getRight() - radius);
}

inline void drawCapsLabel (juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r,
                           juce::Justification j = juce::Justification::centredLeft, float size = 13.0f)
{
    g.setColour (SwellTheme::text.withAlpha (0.88f));
    g.setFont (swellFont (size, false).withExtraKerningFactor (0.08f));
    g.drawText (s.toUpperCase().replace (utf8 ("è"), utf8 ("È")).replace (utf8 ("é"), utf8 ("É")), r, j);
}

class SwellLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SwellLookAndFeel() { applyTheme(); }

    void applyTheme()
    {
        using namespace SwellTheme;
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::TextButton::buttonColourId, panelTop);
        setColour (juce::TextButton::buttonOnColourId, c().accent);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, text);
        setColour (juce::TextEditor::backgroundColourId, well);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::outlineColourId, edge);
        setColour (juce::TextEditor::focusedOutlineColourId, c().accent);
        setColour (juce::TextEditor::highlightColourId, c().accent.withAlpha (0.35f));
        setColour (juce::CaretComponent::caretColourId, c().accent);
        setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ScrollBar::thumbColourId, edgeHi);
        setColour (juce::PopupMenu::backgroundColourId, panelTop);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, c().accent);
        setColour (juce::PopupMenu::highlightedTextColourId, text);
        setColour (juce::TooltipWindow::backgroundColourId, panelTop);
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::AlertWindow::backgroundColourId, panelTop);
        setColour (juce::AlertWindow::textColourId, text);
    }

    // Metal knob: ticks, accent value arc, dark collar, brushed silver cap, accent pointer.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace SwellTheme;
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
        const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto r = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
        const auto c0 = r.getCentre();
        const float radius = size * 0.5f;
        const bool hot = s.isMouseOverOrDragging();
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0 && std::abs (s.getMinimum() + s.getMaximum()) < 1e-3;
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float mid = (startAngle + endAngle) * 0.5f;

        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * i / 10.0f;
            g.setColour (faint.withAlpha (0.8f));
            g.drawLine ({ c0.getPointOnCircumference (radius - 1.0f, a), c0.getPointOnCircumference (radius - 4.0f, a) }, 1.0f);
        }

        const float arcR = radius - 7.0f;
        juce::Path track;
        track.addCentredArc (c0.x, c0.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path arc;
        const float from = bipolar ? mid : startAngle;
        arc.addCentredArc (c0.x, c0.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (c().accent.withAlpha (0.25f));
        g.strokePath (arc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (hot ? c().accentSoft : c().accent);
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto collar = r.reduced (size * 0.2f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (collar.translated (0.0f, 2.5f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3e), collar.getX(), collar.getY(),
                                                 juce::Colour (0xff111113), collar.getX(), collar.getBottom(), false));
        g.fillEllipse (collar);

        const auto cap = collar.reduced (collar.getWidth() * 0.12f);
        juce::ColourGradient metal (juce::Colour (0xffe8e8ea), cap.getCentreX() - cap.getWidth() * 0.25f, cap.getY(),
                                    juce::Colour (0xff6b6b70), cap.getCentreX() + cap.getWidth() * 0.2f, cap.getBottom(), true);
        metal.addColour (0.55, juce::Colour (0xffa9a9ae));
        g.setGradientFill (metal);
        g.fillEllipse (cap);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        for (int i = 1; i < 5; ++i)
            g.drawEllipse (cap.reduced (cap.getWidth() * 0.09f * i), 0.6f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (cap, 0.8f);

        const float inner = cap.getWidth() * 0.5f;
        juce::Path ptr;
        ptr.addRoundedRectangle (-1.4f, -inner + 1.5f, 2.8f, inner * 0.5f, 1.2f);
        g.setColour (c().accent);
        g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c0.x, c0.y));
    }

    // Horizontal fader: dark slot, accent fill, metal thumb.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style != juce::Slider::LinearHorizontal)
            return LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
        const float cy = (float) y + (float) h * 0.5f;
        auto slot = juce::Rectangle<float> ((float) x + 4.0f, cy - 2.5f, (float) w - 8.0f, 5.0f);
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.fillRoundedRectangle (slot, 2.5f);
        g.setColour (SwellTheme::c().accent);
        g.fillRoundedRectangle (slot.withRight (sliderPos), 2.5f);
        const auto thumb = juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ sliderPos, cy });
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (thumb.translated (0.0f, 1.5f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe6e6e8), thumb.getX(), thumb.getY(),
                                                 juce::Colour (0xff76767b), thumb.getX(), thumb.getBottom(), false));
        g.fillEllipse (thumb);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawEllipse (thumb, 0.8f);
        g.setColour (juce::Colour (0xff4a4a4f));
        g.fillEllipse (thumb.reduced (5.5f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        using namespace SwellTheme;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        if (b.getToggleState())
        {
            g.setGradientFill (juce::ColourGradient (c().accent.brighter (0.1f), r.getX(), r.getY(), c().accentDeep, r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (c().accent.brighter (0.3f).withAlpha (0.8f));
            g.drawRoundedRectangle (r, 6.0f, 1.0f);
            return;
        }
        auto top = panelTop.brighter (over ? 0.08f : 0.03f), bot = panelBot;
        if (down) std::swap (top, bot);
        g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bot, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (over ? edgeHi.brighter (0.2f) : edgeHi);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setColour (SwellTheme::text.withAlpha (b.getToggleState() ? 1.0f : 0.85f));
        g.setFont (swellFont (juce::jmin (15.0f, (float) b.getHeight() * 0.45f), b.getToggleState()));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        using namespace SwellTheme;
        auto r = b.getLocalBounds().toFloat();
        auto sw = r.removeFromLeft (40.0f).withSizeKeepingCentre (38.0f, 20.0f);
        const bool on = b.getToggleState();
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (sw, 10.0f);
        if (on)
        {
            g.setColour (c().accent);
            g.fillRoundedRectangle (sw.reduced (1.5f), 8.5f);
        }
        const auto knob = juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ on ? sw.getRight() - 10.0f : sw.getX() + 10.0f, sw.getCentreY() });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe6e6e8), knob.getX(), knob.getY(),
                                                 juce::Colour (0xff7a7a7f), knob.getX(), knob.getBottom(), false));
        g.fillEllipse (knob);
        g.setColour (on || over ? text : dim);
        g.setFont (swellFont (13.0f, on));
        g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedLeft (8), juce::Justification::centredLeft, 1);
    }

    void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override
    {
        auto thumb = vertical ? juce::Rectangle<int> (x + w / 2 - 2, thumbStart, 4, thumbSize)
                              : juce::Rectangle<int> (thumbStart, y + h / 2 - 2, thumbSize, 4);
        g.setColour (SwellTheme::edgeHi.brighter (over || down ? 0.5f : 0.15f));
        g.fillRoundedRectangle (thumb.toFloat(), 2.0f);
    }

    void fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&) override
    {
        g.setColour (SwellTheme::well);
        g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 6.0f);
    }

    void drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e) override
    {
        g.setColour (e.hasKeyboardFocus (true) ? SwellTheme::c().accent : SwellTheme::edge);
        g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 6.0f, 1.0f);
    }
};
