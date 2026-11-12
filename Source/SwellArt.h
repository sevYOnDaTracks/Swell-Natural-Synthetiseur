#pragma once

#include "SwellTheme.h"

// Vector artwork: category icons and the hero illustration behind the sound name.
namespace SwellArt
{
using namespace juce;

inline String keyOf (const String& category)
{
    const String c = category.toLowerCase();
    if (c.contains ("e. piano")) return "epiano";
    if (c.contains ("organ flutes")) return "drawbar";
    for (auto* k : { "piano", "strings", "bass", "guitar", "saxophone", "flute", "organ", "trumpet", "brass", "accordion", "choir", "synth", "perc" })
        if (c.contains (k)) return k;
    return "all";
}

inline void drawIcon (Graphics& g, const String& key, Rectangle<float> r, Colour col)
{
    r = r.reduced (1.0f);
    g.setColour (col);
    const PathStrokeType stroke (1.6f, PathStrokeType::curved, PathStrokeType::rounded);
    Path p;
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();

    if (key == "all")
    {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                g.fillRoundedRectangle (x + w * (0.05f + 0.33f * i), y + h * (0.05f + 0.33f * j), w * 0.24f, h * 0.24f, 1.0f);
        return;
    }
    if (key == "fav") { drawStar (g, r.getCentre(), w * 0.5f, true, col); return; }
    if (key == "piano" || key == "epiano" || key == "drawbar")
    {
        g.drawRoundedRectangle (r.reduced (0.0f, h * 0.15f), 1.5f, 1.5f);
        for (int i = 1; i < 4; ++i)
            g.drawVerticalLine ((int) (x + w * i / 4.0f), y + h * 0.15f, y + h * 0.85f);
        for (int i : { 1, 3 })
            g.fillRect (x + w * i / 4.0f - w * 0.07f, y + h * 0.15f, w * 0.14f, h * 0.38f);
        if (key == "epiano")
        {
            p.startNewSubPath (x, y + h * 0.05f);
            p.cubicTo (x + w * 0.3f, y - h * 0.1f, x + w * 0.6f, y + h * 0.2f, x + w, y + h * 0.02f);
            g.strokePath (p, PathStrokeType (1.2f));
        }
        return;
    }
    if (key == "strings")
    {
        p.addEllipse (x + w * 0.22f, y + h * 0.42f, w * 0.56f, h * 0.5f);
        p.addEllipse (x + w * 0.3f, y + h * 0.15f, w * 0.4f, h * 0.36f);
        g.strokePath (p, stroke);
        g.drawLine (x + w * 0.5f, y, x + w * 0.5f, y + h * 0.9f, 1.4f);
        return;
    }
    if (key == "bass")
    {
        p.addCentredArc (x + w * 0.38f, y + h * 0.42f, w * 0.3f, h * 0.3f, 0.0f, -MathConstants<float>::halfPi, MathConstants<float>::pi * 0.9f, true);
        g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.fillEllipse (x + w * 0.12f, y + h * 0.36f, w * 0.18f, h * 0.18f);
        g.fillEllipse (x + w * 0.8f, y + h * 0.22f, w * 0.13f, h * 0.13f);
        g.fillEllipse (x + w * 0.8f, y + h * 0.5f, w * 0.13f, h * 0.13f);
        return;
    }
    if (key == "guitar")
    {
        p.addEllipse (x + w * 0.05f, y + h * 0.45f, w * 0.48f, h * 0.48f);
        g.strokePath (p, stroke);
        g.drawLine (x + w * 0.4f, y + h * 0.58f, x + w * 0.92f, y + h * 0.08f, 2.2f);
        return;
    }
    if (key == "saxophone")
    {
        p.startNewSubPath (x + w * 0.62f, y);
        p.lineTo (x + w * 0.62f, y + h * 0.62f);
        p.cubicTo (x + w * 0.62f, y + h * 1.02f, x + w * 0.12f, y + h * 1.02f, x + w * 0.16f, y + h * 0.66f);
        g.strokePath (p, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        g.fillEllipse (x + w * 0.06f, y + h * 0.52f, w * 0.24f, h * 0.18f);
        return;
    }
    if (key == "flute")
    {
        g.drawLine (x, y + h * 0.8f, x + w, y + h * 0.2f, 2.4f);
        for (int i = 1; i < 4; ++i)
            g.fillEllipse (x + w * (0.22f + 0.18f * i) - 1.5f, y + h * (0.8f - 0.6f * (0.22f + 0.18f * i)) - 4.5f, 3.0f, 3.0f);
        return;
    }
    if (key == "organ")
    {
        const float hs[] = { 0.55f, 0.8f, 1.0f, 0.8f, 0.55f };
        for (int i = 0; i < 5; ++i)
            g.fillRoundedRectangle (x + w * (0.02f + 0.2f * i), y + h * (1.0f - hs[i]), w * 0.13f, h * hs[i], 1.0f);
        return;
    }
    if (key == "trumpet" || key == "brass")
    {
        p.startNewSubPath (x, y + h * 0.5f);
        p.lineTo (x + w * 0.6f, y + h * 0.5f);
        p.lineTo (x + w, y + h * 0.18f);
        p.lineTo (x + w, y + h * 0.82f);
        p.lineTo (x + w * 0.6f, y + h * 0.5f);
        g.strokePath (p, stroke);
        for (int i : { 2, 3, 4 })
            g.drawVerticalLine ((int) (x + w * 0.1f * i), y + h * 0.28f, y + h * 0.5f);
        return;
    }
    if (key == "accordion")
    {
        p.startNewSubPath (x, y + h * 0.15f);
        for (int i = 1; i <= 6; ++i)
            p.lineTo (x + w * i / 6.0f, y + (i % 2 ? h * 0.85f : h * 0.15f));
        g.strokePath (p, stroke);
        return;
    }
    if (key == "choir")
    {
        g.fillEllipse (x + w * 0.33f, y + h * 0.05f, w * 0.34f, h * 0.34f);
        p.addCentredArc (x + w * 0.5f, y + h * 0.95f, w * 0.38f, h * 0.45f, 0.0f, -MathConstants<float>::halfPi, MathConstants<float>::halfPi, true);
        g.strokePath (p, stroke);
        return;
    }
    if (key == "synth")
    {
        p.startNewSubPath (x, y + h * 0.5f);
        p.lineTo (x + w * 0.2f, y + h * 0.5f);
        p.lineTo (x + w * 0.35f, y + h * 0.1f);
        p.lineTo (x + w * 0.5f, y + h * 0.9f);
        p.lineTo (x + w * 0.65f, y + h * 0.3f);
        p.lineTo (x + w * 0.78f, y + h * 0.5f);
        p.lineTo (x + w, y + h * 0.5f);
        g.strokePath (p, stroke);
        return;
    }
    if (key == "perc")
    {
        g.drawEllipse (x + w * 0.05f, y + h * 0.15f, w * 0.9f, h * 0.3f, 1.5f);
        g.drawLine (x + w * 0.05f, y + h * 0.3f, x + w * 0.05f, y + h * 0.8f, 1.5f);
        g.drawLine (x + w * 0.95f, y + h * 0.3f, x + w * 0.95f, y + h * 0.8f, 1.5f);
        p.addCentredArc (x + w * 0.5f, y + h * 0.8f, w * 0.45f, h * 0.15f, 0.0f, MathConstants<float>::halfPi, MathConstants<float>::pi * 1.5f, true);
        g.strokePath (p, PathStrokeType (1.5f));
        return;
    }
}

// ------------------------------------------------------------------ hero illustration

inline void warmBackdrop (Graphics& g, Rectangle<float> r, Colour accent)
{
    g.setGradientFill (ColourGradient (Colour (0xff0d0b0a), r.getX(), r.getY(), Colour (0xff050505), r.getX(), r.getBottom(), false));
    g.fillRect (r);
    // Bokeh glow from the upper right
    ColourGradient glow (Colour (0xffd47a3a).withAlpha (0.55f), r.getRight() - r.getWidth() * 0.22f, r.getY() + r.getHeight() * 0.18f,
                         Colours::transparentBlack, r.getRight() - r.getWidth() * 0.22f + r.getWidth() * 0.55f, r.getY() + r.getHeight() * 0.18f, true);
    g.setGradientFill (glow);
    g.fillRect (r);
    ColourGradient tint (accent.withAlpha (0.18f), r.getX() + r.getWidth() * 0.15f, r.getBottom(),
                         Colours::transparentBlack, r.getX() + r.getWidth() * 0.15f + r.getWidth() * 0.5f, r.getBottom(), true);
    g.setGradientFill (tint);
    g.fillRect (r);
    Random rnd (7);
    for (int i = 0; i < 14; ++i)
    {
        const float bx = r.getX() + r.getWidth() * (0.45f + 0.55f * rnd.nextFloat());
        const float by = r.getY() + r.getHeight() * 0.5f * rnd.nextFloat();
        const float br = 6.0f + 22.0f * rnd.nextFloat();
        g.setColour (Colour (0xffffb070).withAlpha (0.04f + 0.08f * rnd.nextFloat()));
        g.fillEllipse (bx - br, by - br, br * 2, br * 2);
    }
}

// Keyboard in perspective, lit from the right
inline void heroKeys (Graphics& g, Rectangle<float> r, bool organ)
{
    // Keys run diagonally from lower-left to upper-right; front edge faces the viewer.
    const int keys = 15;
    const juce::Point<float> frontL (r.getX() + r.getWidth() * 0.02f, r.getBottom() + r.getHeight() * 0.08f);
    const juce::Point<float> frontR (r.getRight() + r.getWidth() * 0.05f, r.getY() + r.getHeight() * 0.62f);
    const juce::Point<float> depth (-r.getWidth() * 0.10f, -r.getHeight() * 0.42f);
    auto P = [&] (float u, float v) { return frontL + (frontR - frontL) * u + depth * v; };

    // Case lid behind
    Path lid;
    lid.startNewSubPath (P (0.0f, 1.0f)); lid.lineTo (P (1.0f, 1.0f)); lid.lineTo (P (1.0f, 1.45f)); lid.lineTo (P (0.0f, 1.45f)); lid.closeSubPath();
    g.setGradientFill (ColourGradient (Colour (0xff3a2417), P (1.0f, 1.2f).x, P (1.0f, 1.2f).y, Colour (0xff0a0706), P (0.0f, 1.2f).x, P (0.0f, 1.2f).y, false));
    g.fillPath (lid);
    if (organ)
        for (int i = 0; i < 9; ++i)
        {
            const auto a = P (0.12f + i * 0.09f, 1.25f);
            g.setColour (i % 3 == 0 ? Colour (0xffe8dccb) : Colour (0xff7a2a1f));
            g.fillRoundedRectangle (a.x - 5.0f, a.y - 16.0f, 10.0f, 20.0f, 2.0f);
        }

    // White keys: top surface + front face
    for (int k = 0; k < keys; ++k)
    {
        const float u0 = (float) k / keys + 0.003f, u1 = (float) (k + 1) / keys - 0.003f;
        const float light = 0.3f + 0.7f * (float) k / keys;
        Path top;
        top.startNewSubPath (P (u0, 0.0f)); top.lineTo (P (u1, 0.0f)); top.lineTo (P (u1, 1.0f)); top.lineTo (P (u0, 1.0f)); top.closeSubPath();
        g.setGradientFill (ColourGradient (Colour (0xfff6ecdd).withMultipliedBrightness (light), P (u0, 0.0f).x, P (u0, 0.0f).y,
                                           Colour (0xff9c8c78).withMultipliedBrightness (light), P (u0, 1.0f).x, P (u0, 1.0f).y, false));
        g.fillPath (top);
        Path face;
        face.startNewSubPath (P (u0, 0.0f)); face.lineTo (P (u1, 0.0f));
        face.lineTo (P (u1, 0.0f).translated (0.0f, 9.0f)); face.lineTo (P (u0, 0.0f).translated (0.0f, 9.0f)); face.closeSubPath();
        g.setColour (Colour (0xffb9aa95).withMultipliedBrightness (light * 0.8f));
        g.fillPath (face);
    }
    // Black keys
    const int pattern[] = { 1, 1, 0, 1, 1, 1, 0 };
    for (int k = 0; k < keys - 1; ++k)
    {
        if (! pattern[k % 7]) continue;
        const float uc = (float) (k + 1) / keys, hw = 0.3f / keys;
        Path bk;
        bk.startNewSubPath (P (uc - hw, 0.38f)); bk.lineTo (P (uc + hw, 0.38f)); bk.lineTo (P (uc + hw, 1.0f)); bk.lineTo (P (uc - hw, 1.0f)); bk.closeSubPath();
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillPath (bk, AffineTransform::translation (3.0f, 3.0f));
        g.setGradientFill (ColourGradient (Colour (0xff2b2826), P (uc, 0.38f).x, P (uc, 0.38f).y, Colour (0xff070606), P (uc, 1.0f).x, P (uc, 1.0f).y, false));
        g.fillPath (bk);
        g.setColour (Colour (0xffffc58a).withAlpha (0.35f * uc));
        g.strokePath (bk, PathStrokeType (0.9f));
    }
}

// Fretboard with strings
inline void heroStrings (Graphics& g, Rectangle<float> r, int strings, bool bow)
{
    Path neck;
    const float y0 = r.getY() + r.getHeight() * 0.3f, y1 = r.getBottom() - r.getHeight() * 0.08f;
    neck.startNewSubPath (r.getX() - 20.0f, y0 + 40.0f);
    neck.lineTo (r.getRight() + 20.0f, y0 - 30.0f);
    neck.lineTo (r.getRight() + 20.0f, y1 - 10.0f);
    neck.lineTo (r.getX() - 20.0f, y1 + 50.0f);
    neck.closeSubPath();
    g.setGradientFill (ColourGradient (Colour (0xff3a2316), r.getRight(), y0, Colour (0xff100906), r.getX(), y1, false));
    g.fillPath (neck);
    for (int f = 0; f < 9; ++f)
    {
        const float x = r.getX() + r.getWidth() * (0.08f + f * 0.115f);
        const float t = (x - r.getX()) / r.getWidth();
        g.setColour (Colour (0xffc9b79c).withAlpha (0.25f + 0.5f * t));
        g.drawLine (x, y0 + 40.0f - 70.0f * t, x + 6.0f, y1 + 50.0f - 60.0f * t, 2.0f);
    }
    for (int s = 0; s < strings; ++s)
    {
        const float a = (s + 1.0f) / (strings + 1.0f);
        const float ys = y0 + 40.0f + (y1 - y0 + 10.0f) * a, ye = y0 - 30.0f + (y1 - y0 + 20.0f) * a;
        g.setGradientFill (ColourGradient (Colour (0xff6d6355), r.getX(), ys, Colour (0xfff2dcc0), r.getRight(), ye, false));
        g.drawLine (r.getX() - 20.0f, ys, r.getRight() + 20.0f, ye, 1.0f + 0.35f * s);
    }
    if (bow)
    {
        g.setColour (Colour (0xffe8d9c0).withAlpha (0.65f));
        g.drawLine (r.getX() + r.getWidth() * 0.3f, r.getY() + 10.0f, r.getRight() - r.getWidth() * 0.15f, r.getBottom() - 10.0f, 2.0f);
    }
}

// Brass bell seen from the front, concentric reflections
inline void heroBell (Graphics& g, Rectangle<float> r, bool woodwind)
{
    const auto c = juce::Point<float> (r.getRight() - r.getWidth() * 0.28f, r.getCentreY() + r.getHeight() * 0.05f);
    const float R = r.getHeight() * 0.75f;
    for (int i = 14; i >= 0; --i)
    {
        const float rr = R * (0.12f + 0.88f * i / 14.0f);
        const Colour base = woodwind ? Colour (0xff2a1c12) : Colour (0xffb9832e);
        g.setGradientFill (ColourGradient (base.brighter (0.6f - i * 0.03f).withMultipliedBrightness (0.35f + 0.05f * (14 - i)),
                                           c.x - rr * 0.4f, c.y - rr * 0.5f,
                                           base.darker (0.8f), c.x + rr * 0.6f, c.y + rr * 0.6f, false));
        g.fillEllipse (c.x - rr, c.y - rr * 0.92f, rr * 2, rr * 1.84f);
    }
    g.setColour (Colours::black.withAlpha (0.85f));
    g.fillEllipse (c.x - R * 0.1f, c.y - R * 0.09f, R * 0.2f, R * 0.18f);
    if (woodwind)
        for (int i = 0; i < 6; ++i)
        {
            g.setColour (Colour (0xffd6c2a0).withAlpha (0.8f));
            g.fillEllipse (r.getX() + r.getWidth() * (0.12f + i * 0.08f), r.getBottom() - r.getHeight() * (0.25f + i * 0.05f), 9.0f, 9.0f);
        }
}

// Drum heads
inline void heroDrums (Graphics& g, Rectangle<float> r)
{
    const struct { float x, y, s; } drums[] = { { 0.58f, 0.62f, 0.95f }, { 0.86f, 0.42f, 0.6f }, { 0.32f, 0.8f, 0.55f } };
    for (auto d : drums)
    {
        const float w = r.getHeight() * d.s * 1.2f, h = w * 0.36f;
        const auto c = juce::Point<float> (r.getX() + r.getWidth() * d.x, r.getY() + r.getHeight() * d.y);
        g.setGradientFill (ColourGradient (Colour (0xff6b1d18), c.x - w * 0.5f, c.y, Colour (0xff1a0807), c.x + w * 0.5f, c.y, false));
        g.fillRect (c.x - w * 0.5f, c.y, w, h * 1.1f);
        g.setGradientFill (ColourGradient (Colour (0xfff1e6d4), c.x + w * 0.2f, c.y - h * 0.4f, Colour (0xff6f665a), c.x - w * 0.4f, c.y + h * 0.4f, false));
        g.fillEllipse (c.x - w * 0.5f, c.y - h * 0.5f, w, h);
        g.setColour (Colour (0xffd8d8dc).withAlpha (0.7f));
        g.drawEllipse (c.x - w * 0.5f, c.y - h * 0.5f, w, h, 2.0f);
    }
}

// Light waves (pads, choirs, synths)
inline void heroWaves (Graphics& g, Rectangle<float> r, Colour accent, bool grid)
{
    if (grid)
    {
        g.setColour (accent.withAlpha (0.12f));
        for (int i = 0; i < 12; ++i)
        {
            const float t = i / 11.0f;
            g.drawLine (r.getX() + r.getWidth() * t, r.getBottom(), r.getCentreX() + (t - 0.5f) * r.getWidth() * 0.3f, r.getY() + r.getHeight() * 0.55f, 1.0f);
        }
        for (int i = 0; i < 6; ++i)
        {
            const float yy = r.getY() + r.getHeight() * (0.55f + 0.45f * std::pow (i / 5.0f, 1.8f));
            g.drawHorizontalLine ((int) yy, r.getX(), r.getRight());
        }
    }
    for (int l = 0; l < 7; ++l)
    {
        Path p;
        const float amp = r.getHeight() * (0.08f + 0.03f * l);
        const float base = r.getY() + r.getHeight() * (0.38f + 0.06f * l);
        for (int i = 0; i <= 80; ++i)
        {
            const float t = i / 80.0f;
            const float yy = base + amp * std::sin (t * 7.0f + l * 0.7f) * std::sin (t * 3.1f + l);
            if (i == 0) p.startNewSubPath (r.getX(), yy); else p.lineTo (r.getX() + r.getWidth() * t, yy);
        }
        g.setColour ((l % 2 ? Colour (0xffffb070) : accent).withAlpha (0.18f + 0.08f * (6 - l) / 6.0f));
        g.strokePath (p, PathStrokeType (2.0f + (6 - l) * 0.5f));
    }
}

inline void drawHero (Graphics& g, Rectangle<float> r, const String& category, Colour accent)
{
    Graphics::ScopedSaveState s (g);
    Path clip;
    clip.addRoundedRectangle (r, 8.0f);
    g.reduceClipRegion (clip);
    warmBackdrop (g, r, accent);
    const String k = keyOf (category);
    const auto art = r.withTrimmedLeft (r.getWidth() * 0.18f);
    if (k == "piano" || k == "epiano") heroKeys (g, art, false);
    else if (k == "organ" || k == "drawbar" || k == "accordion") heroKeys (g, art, true);
    else if (k == "guitar") heroStrings (g, art, 6, false);
    else if (k == "bass") heroStrings (g, art, 4, false);
    else if (k == "strings") heroStrings (g, art, 4, true);
    else if (k == "trumpet" || k == "brass" || k == "saxophone") heroBell (g, art, false);
    else if (k == "flute") heroBell (g, art, true);
    else if (k == "perc") heroDrums (g, art);
    else heroWaves (g, art, accent, k == "synth");

    // Legibility shade on the left for the title
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.8f), r.getX(), r.getY(), Colours::transparentBlack, r.getX() + r.getWidth() * 0.6f, r.getY(), false));
    g.fillRect (r);
    g.setGradientFill (ColourGradient (Colours::transparentBlack, r.getX(), r.getCentreY(), Colours::black.withAlpha (0.45f), r.getX(), r.getBottom(), false));
    g.fillRect (r);
}
} // namespace SwellArt
