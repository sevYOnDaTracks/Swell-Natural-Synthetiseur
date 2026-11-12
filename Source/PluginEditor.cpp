#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;
using namespace SwellTheme;

static const SwellPalette& A() { return SwellTheme::c(); }
static String categoryLabel (const String& s) { return s.replace (" & ", " & "); }

// ================================================================== Knob

SwellKnob::SwellKnob (AudioProcessorValueTreeState& state, const String& paramId, const String& cap, float size)
    : caption (cap), captionSize (size)
{
    param = state.getParameter (paramId);
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.onValueChange = [this] { repaint(); };
    slider.onDragStart = [this] { repaint(); };
    slider.onDragEnd = [this] { repaint(); };
    slider.addMouseListener (this, false);
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void SwellKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom ((int) (captionSize + 10));
    const int s = jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (s, s));
}

void SwellKnob::paint (Graphics& g)
{
    const bool hot = slider.isMouseOverOrDragging();
    auto r = getLocalBounds().removeFromBottom ((int) (captionSize + 8));
    if (hot)
    {
        g.setColour (A().accentSoft);
        g.setFont (swellFont (captionSize, true));
        g.drawFittedText (param->getCurrentValueAsText(), r, Justification::centred, 1);
    }
    else
        drawCapsLabel (g, caption, r, Justification::centred, captionSize);
}

// ================================================================== Fader

SwellFader::SwellFader (AudioProcessorValueTreeState& state, const String& paramId, const String& l)
    : letter (l)
{
    param = state.getParameter (paramId);
    slider.setSliderStyle (Slider::LinearHorizontal);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.onValueChange = [this] { repaint(); if (onChange) onChange(); };
    slider.addMouseListener (this, false);
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void SwellFader::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedLeft (28).withTrimmedRight (62));
}

void SwellFader::paint (Graphics& g)
{
    g.setColour (dim);
    g.setFont (swellFont (14.0f));
    g.drawText (letter, getLocalBounds().removeFromLeft (24), Justification::centred);
    g.setColour (slider.isMouseOverOrDragging() ? A().accentSoft : faint.brighter (0.4f));
    g.setFont (swellFont (11.5f));
    g.drawText (param->getCurrentValueAsText(), getLocalBounds().removeFromRight (60), Justification::centredRight);
}

// ================================================================== Category list

int CategoryList::rowY (int code) const
{
    if (code == favourites) return headerH - scroll;
    if (code == all) return headerH + rowH - scroll;
    return headerH + 2 * rowH + 6 + code * rowH - scroll;
}

int CategoryList::maxScroll() const
{
    const int content = headerH + 2 * rowH + 6 + categories.size() * rowH + 10;
    return jmax (0, content - getHeight());
}

void CategoryList::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    scroll = jlimit (0, maxScroll(), scroll - roundToInt (w.deltaY * 160.0f));
    repaint();
}

void CategoryList::paint (Graphics& g)
{
    drawHardwarePanel (g, getLocalBounds().toFloat());
    drawCapsLabel (g, "Library", { 16, 10, getWidth() - 30, 22 }, Justification::centredLeft, 16.0f);

    auto drawRow = [&] (int code, const String& text, const String& iconKey)
    {
        auto row = Rectangle<int> (8, rowY (code), getWidth() - 16, rowH - 2);
        const bool sel = code == selected, hov = code == hover;
        if (sel)
        {
            g.setGradientFill (ColourGradient (A().accent, (float) row.getX(), (float) row.getY(), A().accentDeep, (float) row.getX(), (float) row.getBottom(), false));
            g.fillRoundedRectangle (row.toFloat(), 4.0f);
        }
        else if (hov)
        {
            g.setColour (Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (row.toFloat(), 4.0f);
        }
        auto ir = row.withWidth (row.getHeight()).reduced (3).translated (8, 0).toFloat();
        SwellArt::drawIcon (g, iconKey, ir, sel ? text.isEmpty() ? A().accent : SwellTheme::text : (code == favourites ? A().accent : dim));
        g.setColour (sel ? SwellTheme::text : (hov ? SwellTheme::text : SwellTheme::text.withAlpha (0.82f)));
        g.setFont (swellFont (14.0f, sel));
        g.drawText (text, row.withTrimmedLeft (40), Justification::centredLeft);
        if (code == favourites && favCount > 0)
        {
            g.setFont (swellFont (11.5f));
            g.setColour (sel ? SwellTheme::text : faint.brighter (0.4f));
            g.drawText (String (favCount), row.withTrimmedRight (8), Justification::centredRight);
        }
    };

    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (0, headerH - 2, getWidth(), getHeight() - headerH - 4);
        drawRow (favourites, "Favorites", "fav");
        drawRow (all, "All Sounds", "all");
        g.setColour (edge);
        g.fillRect (16, headerH + 2 * rowH + 2 - scroll, getWidth() - 32, 1);
        for (int i = 0; i < categories.size(); ++i)
            drawRow (i, categories[i], SwellArt::keyOf (categories[i]));
    }
    if (const int ms = maxScroll(); ms > 0)
    {
        const float trackH = (float) (getHeight() - headerH - 12);
        const float thumbH = trackH * (float) getHeight() / (float) (getHeight() + ms);
        const float ty = (float) headerH + (trackH - thumbH) * (float) scroll / (float) ms;
        g.setColour (edgeHi.brighter (0.2f));
        g.fillRoundedRectangle ((float) getWidth() - 7.0f, ty, 3.0f, thumbH, 1.5f);
    }
}

int CategoryList::rowAt (int y) const
{
    if (y < headerH - 2) return none;
    y += scroll;
    if (y >= headerH && y < headerH + rowH) return favourites;
    if (y >= headerH + rowH && y < headerH + 2 * rowH) return all;
    const int base = headerH + 2 * rowH + 6;
    const int i = (y - base) / rowH;
    return (y >= base && i >= 0 && i < categories.size()) ? i : none;
}

void CategoryList::mouseDown (const MouseEvent& e)
{
    const int code = rowAt (e.y);
    if (code != none)
    {
        selected = code;
        repaint();
        if (onSelect) onSelect (code);
    }
}

void CategoryList::mouseMove (const MouseEvent& e)
{
    const int code = rowAt (e.y);
    if (code != hover) { hover = code; repaint(); }
}

// ================================================================== Hero

void HeroPanel::setInstrument (std::shared_ptr<swell::LoadedInstrument> inst, const String& cat)
{
    instrument = std::move (inst);
    category = cat;
    repaint();
}

void HeroPanel::mouseDown (const MouseEvent& e)
{
    if (instrument != nullptr && starArea().contains (e.getPosition()) && onToggleFavourite)
        onToggleFavourite();
}

// Photographic hero per category (Resources/hero/hero_<key>.jpg); vector art as fallback.
static Image heroPhoto (const String& key)
{
    const String res = "hero_" + (key == "all" ? String ("synth") : key) + "_jpg";
    int size = 0;
    if (auto* data = BinaryData::getNamedResource (res.toRawUTF8(), size))
        return ImageCache::getFromMemory (data, size);
    return {};
}

static void drawHeroPhoto (Graphics& g, Rectangle<float> r, const String& category)
{
    const auto img = heroPhoto (SwellArt::keyOf (category));
    if (! img.isValid()) { SwellArt::drawHero (g, r, category, A().accent); return; }
    Graphics::ScopedSaveState s (g);
    Path clip;
    clip.addRoundedRectangle (r, 8.0f);
    g.reduceClipRegion (clip);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.setOpacity (1.0f);
    g.drawImage (img, r, RectanglePlacement (RectanglePlacement::xRight | RectanglePlacement::yMid | RectanglePlacement::fillDestination));
    // Legibility shade on the left for the title, and at the bottom
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.85f), r.getX(), r.getY(), Colours::transparentBlack, r.getX() + r.getWidth() * 0.62f, r.getY(), false));
    g.fillRect (r);
    g.setGradientFill (ColourGradient (Colours::transparentBlack, r.getX(), r.getCentreY(), Colours::black.withAlpha (0.35f), r.getX(), r.getBottom(), false));
    g.fillRect (r);
}

void HeroPanel::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    drawHardwarePanel (g, r);
    const auto art = r.reduced (3.0f);
    drawHeroPhoto (g, art, category);

    const String title = instrument != nullptr ? instrument->name : String ("Swell");
    g.setFont (serifFont (40.0f));
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawFittedText (title, Rectangle<int> (55, 24, getWidth() - 130, 50), Justification::centredLeft, 1);
    g.setColour (text);
    g.drawFittedText (title, Rectangle<int> (54, 22, getWidth() - 130, 50), Justification::centredLeft, 1);
    g.setColour (dim);
    g.setFont (swellFont (15.0f).withExtraKerningFactor (0.18f));
    g.drawText (category.toUpperCase(), Rectangle<int> (56, 72, getWidth() - 130, 22), Justification::centredLeft);

    if (instrument != nullptr)
        drawStar (g, starArea().toFloat().getCentre(), 12.0f, favourite, favourite ? A().accent : text.withAlpha (0.85f));

    if (status.isNotEmpty())
    {
        g.setColour (instrument != nullptr ? A().accentSoft : text);
        g.setFont (swellFont (14.0f, true));
        g.drawFittedText (status, getLocalBounds().reduced (56, 18).withTrimmedTop (getHeight() - 70), Justification::bottomLeft, 2);
    }
}

// ================================================================== Sample view (Edit page)

void SampleView::paint (Graphics& g)
{
    auto r = getLocalBounds();
    drawHardwarePanel (g, r.toFloat());
    r = r.reduced (18, 14);
    drawCapsLabel (g, "Sample", r.removeFromTop (22), Justification::centredLeft, 15.0f);
    if (instrument != nullptr)
    {
        g.setColour (text);
        g.setFont (serifFont (24.0f));
        g.drawText (instrument->name, r.removeFromTop (34), Justification::centredLeft);
    }
    auto info = r.removeFromBottom (22);
    auto strip = r.removeFromBottom (14);
    r.removeFromBottom (10);
    auto scope = r.toFloat();
    drawWell (g, scope, 6.0f);
    g.setColour (Colours::white.withAlpha (0.04f));
    for (int i = 1; i < 16; ++i)
        g.drawVerticalLine ((int) (scope.getX() + scope.getWidth() * i / 16.0f), scope.getY() + 4, scope.getBottom() - 4);
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (scope.getY() + scope.getHeight() * i / 4.0f), scope.getX() + 4, scope.getRight() - 4);

    if (instrument == nullptr || instrument->peaks.empty())
        return;

    const auto& pk = instrument->peaks;
    const float mid = scope.getCentreY(), amp = scope.getHeight() * 0.42f;
    Path wave;
    wave.startNewSubPath (scope.getX() + 6, mid);
    for (size_t i = 0; i < pk.size(); ++i)
        wave.lineTo (scope.getX() + 6 + (scope.getWidth() - 12) * i / (float) (pk.size() - 1), mid - pk[i] * amp);
    for (size_t i = pk.size(); i-- > 0;)
        wave.lineTo (scope.getX() + 6 + (scope.getWidth() - 12) * i / (float) (pk.size() - 1), mid + pk[i] * amp);
    wave.closeSubPath();
    g.setGradientFill (ColourGradient (A().accent.withAlpha (0.55f), 0, scope.getY(), A().accentDeep.withAlpha (0.35f), 0, mid, true));
    g.fillPath (wave);
    g.setColour (A().accent);
    g.strokePath (wave, PathStrokeType (1.2f));

    const int lo = 21, hi = 108;
    const float kw = strip.getWidth() / (float) (hi - lo + 1);
    int idx = 0;
    std::vector<std::pair<int, int>> drawn;
    for (const auto& z : instrument->zones)
    {
        if (z.info.role != swell::ZoneInfo::main || z.info.lowVel > 90 || z.info.highVel < 90) continue;
        const std::pair<int, int> range { z.info.lowKey, z.info.highKey };
        if (std::find (drawn.begin(), drawn.end(), range) != drawn.end()) continue;
        drawn.push_back (range);
        const int a = jmax (lo, z.info.lowKey), b = jmin (hi, z.info.highKey);
        if (b < a) continue;
        g.setColour ((idx++ % 2 == 0) ? A().accent : A().accentDeep);
        g.fillRoundedRectangle (strip.getX() + (a - lo) * kw, (float) strip.getY(), (b - a + 1) * kw - 1.5f, (float) strip.getHeight(), 2.0f);
    }

    StringArray parts;
    int mainZones = 0;
    for (const auto& z : instrument->zones) mainZones += z.info.role == swell::ZoneInfo::main ? 1 : 0;
    parts.add (String (mainZones) + " zones");
    parts.add (String (instrument->displaySampleRate / 1000.0, 1) + " kHz");
    parts.add (String (instrument->velocityLayers) + " velocity layer" + (instrument->velocityLayers > 1 ? "s" : ""));
    if (instrument->numLoops > 0) parts.add (String (instrument->numLoops) + " loops");
    if (instrument->roundRobin) parts.add ("round-robin");
    if (instrument->hasSlide) parts.add ("S.Art slides");
    if (instrument->hasModWheel) parts.add ("mod-wheel articulation");
    if (instrument->hasLayers) parts.add ("layers");
    if (instrument->oneShot) parts.add ("one-shot");
    parts.add (String (roundToInt ((double) instrument->memoryBytes / 1048576.0)) + " MB");
    g.setColour (dim);
    g.setFont (swellFont (12.5f));
    g.drawFittedText (parts.joinIntoString (utf8 ("  ·  ")), info, Justification::centredLeft, 1);
}

// ================================================================== Envelope graph

void EnvelopeGraph::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawWell (g, r, 6.0f);
    g.setColour (Colours::white.withAlpha (0.035f));
    for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 8.0f), r.getY() + 3, r.getBottom() - 3);
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * i / 4.0f), r.getX() + 3, r.getRight() - 3);
    r = r.reduced (12.0f, 10.0f);

    auto val = [this] (const char* id) { return state.getRawParameterValue (id)->load(); };
    Path p;
    std::vector<Point<float>> pts;
    if (mode == 0)
    {
        const float a = val ("attack"), d = val ("decay"), s = val ("sustain"), rel = val ("release");
        auto tx = [] (float t) { return std::sqrt (jlimit (0.0f, 10.0f, t) / 10.0f); };
        const float wa = 0.06f + 0.24f * tx (a), wd = 0.06f + 0.24f * tx (d), wr = 0.06f + 0.24f * tx (rel);
        const float hold = 0.18f;
        const float total = wa + wd + hold + wr;
        float x = r.getX();
        const float W = r.getWidth() / total;
        pts.push_back ({ x, r.getBottom() });
        x += wa * W; pts.push_back ({ x, r.getY() });
        x += wd * W; pts.push_back ({ x, r.getBottom() - s * r.getHeight() });
        x += hold * W; pts.push_back ({ x, r.getBottom() - s * r.getHeight() });
        x += wr * W; pts.push_back ({ x, r.getBottom() });
        p.startNewSubPath (pts[0]);
        for (size_t i = 1; i < pts.size(); ++i) p.lineTo (pts[i]);
    }
    else
    {
        const float cut = val ("cutoff"), res = val ("reso");
        const float fx = std::log (cut / 40.0f) / std::log (20000.0f / 40.0f);
        const float peak = 0.25f * res;
        p.startNewSubPath (r.getX(), r.getY() + r.getHeight() * 0.35f);
        for (int i = 1; i <= 100; ++i)
        {
            const float t = i / 100.0f;
            const float d = (t - fx) * 8.0f;
            float y = 0.35f;
            if (d > 0) y = 0.35f + d * d * 0.4f;
            y -= peak * std::exp (-d * d * 6.0f);
            p.lineTo (r.getX() + r.getWidth() * t, r.getY() + r.getHeight() * jlimit (0.0f, 1.0f, y));
        }
        pts.push_back ({ r.getX() + r.getWidth() * fx, r.getY() + r.getHeight() * (0.35f - peak) });
    }

    Path fill (p);
    fill.lineTo (p.getCurrentPosition().x, r.getBottom());
    fill.lineTo (r.getX(), r.getBottom());
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (A().accent.withAlpha (0.55f), 0, r.getY(), A().accentDeep.withAlpha (0.15f), 0, r.getBottom(), false));
    g.fillPath (fill);
    g.setColour (A().accent);
    g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::mitered, PathStrokeType::rounded));
    for (auto pt : pts)
    {
        g.setColour (text);
        g.fillEllipse (Rectangle<float> (8.0f, 8.0f).withCentre (pt));
        g.setColour (A().accent);
        g.drawEllipse (Rectangle<float> (8.0f, 8.0f).withCentre (pt), 1.5f);
    }
}

// ================================================================== Keyboard

void SwellKeyboard::drawWhiteNote (int note, Graphics& g, Rectangle<float> area, bool down, bool over, Colour, Colour)
{
    auto k = area.reduced (0.6f, 0.0f);
    Colour top (0xfff2efe9), bot (0xffd8d2c8);
    if (isRoot (note)) { top = top.interpolatedWith (A().accentSoft, 0.18f); bot = bot.interpolatedWith (A().accentSoft, 0.18f); }
    if (over) { top = top.darker (0.04f); bot = bot.darker (0.04f); }
    if (down) { top = A().accentSoft; bot = A().accent; }
    g.setGradientFill (ColourGradient (top, k.getX(), k.getY(), bot, k.getX(), k.getBottom(), false));
    g.fillRoundedRectangle (k.withTrimmedTop (-4.0f), 3.0f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (k.withTrimmedTop (-4.0f), 3.0f, 0.7f);
    g.setColour (Colours::white.withAlpha (0.5f));
    g.drawVerticalLine ((int) k.getX() + 1, k.getY(), k.getBottom() - 3);
    if (note % 12 == 0)
    {
        g.setColour (down ? text : Colour (0xff8a857e));
        g.setFont (swellFont (9.5f));
        g.drawText (MidiMessage::getMidiNoteName (note, true, true, 3), k.removeFromBottom (14.0f), Justification::centred);
    }
}

void SwellKeyboard::drawBlackNote (int note, Graphics& g, Rectangle<float> area, bool down, bool over, Colour)
{
    auto k = area.withTrimmedTop (-4.0f);
    Colour top (0xff2c2a29), bot (0xff050505);
    if (isRoot (note)) top = top.interpolatedWith (A().accentDeep, 0.5f);
    if (over) top = top.brighter (0.2f);
    if (down) { top = A().accent; bot = A().accentDeep; }
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (k.translated (1.5f, 1.5f), 2.5f);
    g.setGradientFill (ColourGradient (bot, k.getX(), k.getY(), top, k.getX(), k.getBottom(), false));
    g.fillRoundedRectangle (k, 2.5f);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.fillRoundedRectangle (k.reduced (k.getWidth() * 0.2f, 0.0f).withTrimmedBottom (k.getHeight() * 0.25f).withTrimmedTop (4.0f), 2.0f);
}

// ================================================================== Icon buttons

void IconButton::paintButton (Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool chevron = icon == "prev" || icon == "next";
    if (! chevron)
    {
        g.setGradientFill (ColourGradient (panelTop.brighter (over ? 0.1f : 0.04f), r.getX(), r.getY(), panelBot, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (over ? edgeHi.brighter (0.3f) : edgeHi);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }
    const auto c = r.getCentre();
    g.setColour (down ? A().accent : (over ? SwellTheme::text : SwellTheme::text.withAlpha (chevron ? 0.75f : 0.85f)));
    if (icon == "settings")
    {
        Path gear;
        const float R = jmin (r.getWidth(), r.getHeight()) * 0.3f;
        for (int i = 0; i < 8; ++i)
        {
            const float a = i * MathConstants<float>::twoPi / 8.0f;
            gear.addRectangle (-R * 0.18f, -R * 1.25f, R * 0.36f, R * 0.5f);
            gear.applyTransform (AffineTransform::rotation (MathConstants<float>::twoPi / 8.0f));
            ignoreUnused (a);
        }
        gear.addEllipse (-R, -R, R * 2, R * 2);
        g.fillPath (gear, AffineTransform::translation (c));
        g.setColour (panelTop);
        g.fillEllipse (Rectangle<float> (R * 0.8f, R * 0.8f).withCentre (c));
    }
    else if (icon == "search")
    {
        Path m;
        m.addEllipse (c.x - 7.0f, c.y - 7.0f, 12.0f, 12.0f);
        m.startNewSubPath (c.x + 3.5f, c.y + 3.5f);
        m.lineTo (c.x + 8.0f, c.y + 8.0f);
        g.strokePath (m, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    else if (chevron)
    {
        Path ch;
        const float s = icon == "prev" ? -1.0f : 1.0f;
        ch.startNewSubPath (c.x - 5.0f * s, c.y - 11.0f);
        ch.lineTo (c.x + 5.0f * s, c.y);
        ch.lineTo (c.x - 5.0f * s, c.y + 11.0f);
        g.strokePath (ch, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }
}

// ================================================================== Settings

static const float scales[] = { 0.75f, 0.9f, 1.0f, 1.25f, 1.5f };

SettingsPanel::SettingsPanel (SwellEditor& e) : editor (e)
{
    changeLibrary.onClick = [this] { editor.getContent().chooseLibrary(); };
    close.onClick = [this] { editor.showSettings (false); };
    addAndMakeVisible (changeLibrary);
    addAndMakeVisible (close);
    for (float s : scales)
    {
        auto* b = sizeButtons.add (new TextButton (String (roundToInt (s * 100.0f)) + " %"));
        b->onClick = [this, s] { editor.setScale (s); refresh(); };
        addAndMakeVisible (b);
    }
    for (int i = 0; i < SwellTheme::numThemes; ++i)
    {
        auto* b = themeButtons.add (new TextButton (SwellTheme::displayName (i)));
        b->onClick = [this, i] { editor.setTheme (i); refresh(); };
        addAndMakeVisible (b);
    }
}

void SettingsPanel::refresh()
{
    changeLibrary.setButtonText (editor.getProcessor().getBank().isLoaded() ? "Change..." : String::fromUTF8 ("Choose folder…"));
    const float sc = editor.getProcessor().getUiScale();
    for (int i = 0; i < sizeButtons.size(); ++i)
        sizeButtons[i]->setToggleState (std::abs (scales[i] - sc) < 0.01f, dontSendNotification);
    for (int i = 0; i < themeButtons.size(); ++i)
        themeButtons[i]->setToggleState (i == SwellTheme::currentIndex, dontSendNotification);
    repaint();
}

void SettingsPanel::mouseDown (const MouseEvent& e)
{
    if (! card.contains (e.getPosition()))
        editor.showSettings (false);
}

static void drawSettingsFooter (Graphics&, Rectangle<int> card);

void SettingsPanel::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.65f));
    drawHardwarePanel (g, card.toFloat(), 10.0f);
    auto r = card.reduced (28, 22);
    g.setColour (text);
    g.setFont (serifFont (26.0f));
    const auto& bank = editor.getProcessor().getBank();
    g.drawText (bank.isLoaded() ? "Settings" : "Choose your sound library", r.removeFromTop (34), Justification::centredLeft);
    r.removeFromTop (12);

    drawCapsLabel (g, "Swell Library location", r.removeFromTop (18), Justification::centredLeft, 12.5f);
    r.removeFromTop (6);
    auto pathRow = r.removeFromTop (36);
    pathRow.removeFromRight (130);
    drawWell (g, pathRow.toFloat());
    g.setColour (bank.isLoaded() ? text : dim);
    g.setFont (swellFont (13.0f));
    g.drawFittedText (bank.isLoaded() ? bank.root.getFullPathName() : String ("Select the extracted Swell Library folder"), pathRow.reduced (12, 0), Justification::centredLeft, 1);
    r.removeFromTop (6);
    g.setColour (faint.brighter (0.3f));
    g.setFont (swellFont (12.0f));
    g.drawText (bank.isLoaded() ? String ((int) bank.sounds.size()) + " sounds in " + String ((int) bank.categories.size()) + " categories" : String ("The folder must contain Swell Library.swellbank and Samples."),
                r.removeFromTop (16), Justification::centredLeft);
    r.removeFromTop (18);
    drawCapsLabel (g, "Interface size", r.removeFromTop (18), Justification::centredLeft, 12.5f);
    r.removeFromTop (6 + 36 + 18);
    drawCapsLabel (g, "Theme", r.removeFromTop (18), Justification::centredLeft, 12.5f);
    drawSettingsFooter (g, card);
}

static void drawSettingsFooter (Graphics& g, Rectangle<int> card)
{
    g.setColour (faint.brighter (0.4f));
    g.setFont (swellFont (12.0f));
    g.drawText (String ("Swell - Natural Synthetiser v") + JucePlugin_VersionString + utf8 ("  \xc2\xb7  by @sevyondatracks"),
                card.getX() + 28, card.getBottom() - 22 - 36, card.getWidth() - 56 - 130, 36, Justification::centredLeft);
}

void SettingsPanel::resized()
{
    card = getLocalBounds().withSizeKeepingCentre (600, 430);
    auto r = card.reduced (28, 22);
    r.removeFromTop (34 + 12 + 18 + 6);
    auto pathRow = r.removeFromTop (36);
    changeLibrary.setBounds (pathRow.removeFromRight (120));
    r.removeFromTop (6 + 16 + 18 + 18 + 6);
    auto sizes = r.removeFromTop (36);
    const int sw = sizes.getWidth() / sizeButtons.size();
    for (auto* b : sizeButtons) b->setBounds (sizes.removeFromLeft (sw).reduced (3, 0));
    r.removeFromTop (18 + 18 + 6);
    auto themes = r.removeFromTop (36);
    const int tw = themes.getWidth() / themeButtons.size();
    for (auto* b : themeButtons) b->setBounds (themes.removeFromLeft (tw).reduced (3, 0));
    close.setBounds (card.getRight() - 28 - 120, card.getBottom() - 22 - 36, 120, 36);
    refresh();
}

// ================================================================== Content

SwellKnob* SwellContent::knob (OwnedArray<SwellKnob>& arr, const char* id, const char* caption, float size)
{
    auto* k = arr.add (new SwellKnob (proc.apvts, id, utf8 (caption), size));
    addAndMakeVisible (k);
    return k;
}

SwellContent::SwellContent (SwellProcessor& p, SwellEditor& e)
    : proc (p), editor (e), envGraph (p.apvts), keyboard (p.keyboardState)
{
    logo = ImageCache::getFromMemory (BinaryData::swell_mark_png, BinaryData::swell_mark_pngSize);

    const char* tabNames[] = { "Sounds", "Edit", "Effects" };
    for (int i = 0; i < 3; ++i)
    {
        tabs[i].setButtonText (tabNames[i]);
        tabs[i].onClick = [this, i] { showPage (i); };
        addAndMakeVisible (tabs[i]);
    }
    settingsButton.setTooltip ("Settings");
    settingsButton.onClick = [this] { editor.showSettings (true); };
    addAndMakeVisible (settingsButton);

    // Library + list
    categoryList.onSelect = [this] (int c) { showCategory (c); };
    addAndMakeVisible (categoryList);
    soundList.setModel (this);
    soundList.setRowHeight (27);
    soundList.setOutlineThickness (0);
    addAndMakeVisible (soundList);

    searchBox.setTextToShowWhenEmpty ("Search all sounds", faint.brighter (0.3f));
    searchBox.setFont (swellFont (14.0f));
    searchBox.setIndents (10, 7);
    searchBox.onTextChange = [this] { refreshSoundList(); };
    searchBox.onEscapeKey = [this] { searchBox.clear(); setSearchVisible (false); refreshSoundList(); };
    searchBox.onReturnKey = [this] { if (! visibleSounds.empty()) proc.selectSound (visibleSounds.front()); };
    addChildComponent (searchBox);
    searchButton.setTooltip ("Search all sounds");
    searchButton.onClick = [this] { setSearchVisible (! searchVisible); };
    addAndMakeVisible (searchButton);

    addAndMakeVisible (licenceBadge);

    // Hero
    hero.onToggleFavourite = [this] { if (auto i = proc.getInstrument()) toggleFavourite (i->soundId); };
    addAndMakeVisible (hero);
    prevButton.onClick = [this] { step (-1); };
    nextButton.onClick = [this] { step (1); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    // Macros
    knob (macroKnobs, "cutoff", "Cutoff", 14.0f);
    knob (macroKnobs, "reso", "Resonance", 14.0f);
    knob (macroKnobs, "attack", "Attack", 14.0f);
    knob (macroKnobs, "release", "Release", 14.0f);

    // Envelope panel
    const char* envNames[] = { "AMP", "FILTER" };
    for (int i = 0; i < 2; ++i)
    {
        envTabs[i].setButtonText (envNames[i]);
        envTabs[i].onClick = [this, i] { setEnvTab (i); };
        addAndMakeVisible (envTabs[i]);
    }
    for (auto [id, l] : { std::pair<const char*, const char*> { "attack", "A" }, { "decay", "D" }, { "sustain", "S" }, { "release", "R" } })
    {
        auto* f = ampFaders.add (new SwellFader (p.apvts, id, l));
        f->onChange = [this] { envGraph.repaint(); };
        addAndMakeVisible (f);
    }
    for (auto [id, l] : { std::pair<const char*, const char*> { "cutoff", "C" }, { "reso", "Q" } })
    {
        auto* f = filterFaders.add (new SwellFader (p.apvts, id, l));
        f->onChange = [this] { envGraph.repaint(); };
        addChildComponent (f);
    }
    addAndMakeVisible (envGraph);

    // Play panel
    const char* modes[] = { "Poly", "Mono", "Legato" };
    for (int i = 0; i < 3; ++i)
    {
        modeButtons[i].setButtonText (modes[i]);
        modeButtons[i].onClick = [this, i] { if (modeAttachment) modeAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (modeButtons[i]);
    }
    modeAttachment = std::make_unique<ParameterAttachment> (*p.apvts.getParameter ("mode"),
        [this] (float v) { currentMode = roundToInt (v); updateModeButtons(); });
    modeAttachment->sendInitialUpdate();
    slideArtToggle.setTooltip (utf8 ("S.Art! guitars: hard-played notes trigger the real slide samples"));
    addAndMakeVisible (slideArtToggle);
    slideArtAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "slideart", slideArtToggle);
    octaveAttachment = std::make_unique<ParameterAttachment> (*p.apvts.getParameter ("octave"),
        [this] (float v) { currentOctave = roundToInt (v); repaint (octLabel); });
    octaveAttachment->sendInitialUpdate();
    octDown.onClick = [this] { octaveAttachment->setValueAsCompleteGesture ((float) jmax (-2, currentOctave - 1)); };
    octUp.onClick = [this] { octaveAttachment->setValueAsCompleteGesture ((float) jmin (2, currentOctave + 1)); };
    addAndMakeVisible (octDown);
    addAndMakeVisible (octUp);
    knob (playKnobs, "glide", "Slide", 12.0f);

    // Effects panel (Sounds page)
    knob (fxKnobs, "chorus", "Chorus", 12.0f);
    knob (fxKnobs, "reverb", "Reverb", 12.0f);
    knob (fxKnobs, "drive", "Drive", 12.0f);

    // Edit page
    addAndMakeVisible (sampleView);
    for (auto [id, c] : { std::pair<const char*, const char*> { "start", "Start" }, { "tune", "Tune" }, { "velsens", "Velocity" },
                          { "attack", "Attack" }, { "decay", "Decay" }, { "sustain", "Sustain" },
                          { "release", "Release" }, { "cutoff", "Cutoff" }, { "reso", "Resonance" } })
        knob (editKnobs, id, c, 12.5f);

    // Effects page
    knob (bigFxKnobs, "drive", "Drive", 15.0f);
    knob (bigFxKnobs, "chorus", "Chorus", 15.0f);
    knob (bigFxKnobs, "reverb", "Reverb", 15.0f);

    // Keyboard block
    knob (outKnobs, "volume", "Volume", 12.0f);
    knob (outKnobs, "pan", "Pan", 12.0f);
    keyboard.setAvailableRange (24, 96);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setOctaveForMiddleC (3);
    keyboard.setWantsKeyboardFocus (false);
    keyboard.setBlackNoteLengthProportion (0.62f);
    addAndMakeVisible (keyboard);

    setSize (baseW, baseH);
    rebuildCategories();
    syncToSelection();
    setEnvTab (0);
    showPage (soundsPage);
    startTimerHz (15);
}

SwellContent::~SwellContent()
{
    stopTimer();
    soundList.setModel (nullptr);
}

void SwellContent::showPage (int p)
{
    page = p;
    for (int i = 0; i < 3; ++i) tabs[i].setToggleState (i == p, dontSendNotification);

    const bool s = p == soundsPage, ed = p == editPage, fx = p == effectsPage;
    for (Component* c : std::initializer_list<Component*> { &categoryList, &soundList, &searchButton, &hero, &prevButton, &nextButton,
                                                            &envTabs[0], &envTabs[1], &envGraph, &modeButtons[0], &modeButtons[1], &modeButtons[2],
                                                            &slideArtToggle, &octDown, &octUp })
        c->setVisible (s);
    searchBox.setVisible (s && searchVisible);
    for (auto* k : macroKnobs) k->setVisible (s);
    for (auto* k : playKnobs) k->setVisible (s);
    for (auto* k : fxKnobs) k->setVisible (s);
    for (auto* f : ampFaders) f->setVisible (s && envTab == 0);
    for (auto* f : filterFaders) f->setVisible (s && envTab == 1);
    sampleView.setVisible (ed);
    for (auto* k : editKnobs) k->setVisible (ed);
    for (auto* k : bigFxKnobs) k->setVisible (fx);
    repaint();
}

void SwellContent::setEnvTab (int t)
{
    envTab = t;
    for (int i = 0; i < 2; ++i) envTabs[i].setToggleState (i == t, dontSendNotification);
    for (auto* f : ampFaders) f->setVisible (page == soundsPage && t == 0);
    for (auto* f : filterFaders) f->setVisible (page == soundsPage && t == 1);
    envGraph.setMode (t);
}

void SwellContent::setSearchVisible (bool v)
{
    searchVisible = v;
    searchBox.setVisible (v && page == soundsPage);
    if (v) searchBox.grabKeyboardFocus();
    else { searchBox.clear(); refreshSoundList(); }
    repaint (listArea);
}

void SwellContent::updateModeButtons()
{
    for (int i = 0; i < 3; ++i)
        modeButtons[i].setToggleState (i == currentMode, dontSendNotification);
}

void SwellContent::themeChanged()
{
    keyboard.repaint();
    repaint();
}

void SwellContent::rebuildCategories()
{
    StringArray names;
    for (auto& c : proc.getBank().categories)
        names.add (categoryLabel (c.name));
    categoryList.setCategories (names);
    categoryList.setFavouriteCount (proc.getFavourites().size());
}

void SwellContent::syncToSelection()
{
    const int sel = proc.getSelectedSound();
    const auto& bank = proc.getBank();
    if (sel >= 0 && sel < (int) bank.sounds.size() && searchBox.getText().isEmpty() && currentCategory >= 0)
        currentCategory = bank.sounds[(size_t) sel].category;
    refreshSoundList();
    shownSelection = sel;
}

void SwellContent::showCategory (int code)
{
    currentCategory = code;
    if (searchVisible) setSearchVisible (false);
    refreshSoundList();
    soundList.scrollToEnsureRowIsOnscreen (0);
}

void SwellContent::refreshSoundList()
{
    const auto& bank = proc.getBank();
    visibleSounds.clear();
    const String q = searchBox.getText().trim();

    if (q.isNotEmpty())
    {
        StringArray words;
        words.addTokens (q, " ", "");
        words.removeEmptyStrings();
        for (size_t i = 0; i < bank.sounds.size(); ++i)
        {
            const String hay = bank.sounds[i].name + " " + bank.categories[(size_t) bank.sounds[i].category].name;
            bool ok = true;
            for (auto& w : words) ok = ok && hay.containsIgnoreCase (w);
            if (ok) visibleSounds.push_back ((int) i);
        }
        categoryList.setSelected (CategoryList::none);
    }
    else
    {
        if (currentCategory == CategoryList::favourites)
        {
            for (auto& id : proc.getFavourites())
                if (const int idx = bank.findSound (id); idx >= 0) visibleSounds.push_back (idx);
        }
        else if (currentCategory == CategoryList::all)
        {
            for (size_t i = 0; i < bank.sounds.size(); ++i) visibleSounds.push_back ((int) i);
        }
        else if (currentCategory >= 0 && currentCategory < (int) bank.categories.size())
            visibleSounds = bank.categories[(size_t) currentCategory].sounds;
        categoryList.setSelected (currentCategory);
    }

    soundList.updateContent();
    const auto it = std::find (visibleSounds.begin(), visibleSounds.end(), proc.getSelectedSound());
    if (it != visibleSounds.end())
        soundList.scrollToEnsureRowIsOnscreen ((int) (it - visibleSounds.begin()));
    soundList.repaint();
    repaint (listArea);
}

void SwellContent::paintListBoxItem (int row, Graphics& g, int w, int h, bool)
{
    if (row < 0 || row >= (int) visibleSounds.size()) return;
    const int idx = visibleSounds[(size_t) row];
    const bool sel = idx == proc.getSelectedSound();
    const auto& snd = proc.getBank().sounds[(size_t) idx];
    const bool fav = proc.isFavourite (snd.id);
    const bool showCat = searchBox.getText().trim().isNotEmpty() || currentCategory < 0;

    auto r = Rectangle<int> (2, 1, w - 4, h - 2);
    if (sel)
    {
        g.setGradientFill (ColourGradient (A().accent, 0.0f, (float) r.getY(), A().accentDeep, 0.0f, (float) r.getBottom(), false));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
    }
    else
    {
        g.setColour (Colours::white.withAlpha (0.045f));
        g.fillRect (12, h - 1, w - 24, 1);
    }
    drawStar (g, { 22.0f, h * 0.5f }, 7.5f, fav, fav ? (sel ? text : A().accent) : (sel ? text.withAlpha (0.9f) : faint.brighter (0.4f)));

    auto tr = r.withTrimmedLeft (44).withTrimmedRight (10);
    if (showCat)
    {
        g.setColour (sel ? text.withAlpha (0.8f) : faint.brighter (0.3f));
        g.setFont (swellFont (11.0f));
        g.drawText (proc.getBank().categories[(size_t) snd.category].name, tr.removeFromRight (100), Justification::centredRight);
    }
    g.setColour (sel ? text : text.withAlpha (0.86f));
    g.setFont (swellFont (14.5f, sel));
    g.drawText (snd.name, tr, Justification::centredLeft);
}

void SwellContent::listBoxItemClicked (int row, const MouseEvent& e)
{
    if (row < 0 || row >= (int) visibleSounds.size()) return;
    const int idx = visibleSounds[(size_t) row];
    if (e.x < 38)
        toggleFavourite (proc.getBank().sounds[(size_t) idx].id);
    else
        proc.selectSound (idx);
    soundList.repaint();
}

void SwellContent::toggleFavourite (const String& id)
{
    proc.toggleFavourite (id);
    categoryList.setFavouriteCount (proc.getFavourites().size());
    if (auto inst = proc.getInstrument())
        hero.setFavourite (proc.isFavourite (inst->soundId));
    if (currentCategory == CategoryList::favourites && searchBox.getText().isEmpty())
        refreshSoundList();
    soundList.repaint();
}

void SwellContent::step (int delta)
{
    const auto& bank = proc.getBank();
    if (! bank.isLoaded()) return;
    const auto it = std::find (visibleSounds.begin(), visibleSounds.end(), proc.getSelectedSound());
    if (it != visibleSounds.end())
    {
        const int n = (int) visibleSounds.size();
        const int pos = ((int) (it - visibleSounds.begin()) + delta + n) % n;
        proc.selectSound (visibleSounds[(size_t) pos]);
    }
    else
    {
        const int n = (int) bank.sounds.size();
        proc.selectSound (((proc.getSelectedSound() + delta) % n + n) % n);
    }
    syncToSelection();
}

void SwellContent::chooseLibrary()
{
    chooser = std::make_unique<FileChooser> ("Swell Library folder", proc.getBank().root, "*", true);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                          [this] (const FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f == File()) return;
        if (proc.setLibraryFolder (f))
        {
            rebuildCategories();
            syncToSelection();
            editor.showSettings (false);
        }
        else
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Swell", proc.getBankError());
    });
}

void SwellContent::timerCallback()
{
    auto inst = proc.getInstrument();
    if (inst != shownInstrument)
    {
        shownInstrument = inst;
        String cat;
        if (inst != nullptr)
        {
            const auto& bank = proc.getBank();
            if (inst->soundIndex >= 0 && inst->soundIndex < (int) bank.sounds.size())
                cat = bank.categories[(size_t) bank.sounds[(size_t) inst->soundIndex].category].name;
        }
        hero.setInstrument (inst, cat);
        hero.setFavourite (inst != nullptr && proc.isFavourite (inst->soundId));
        sampleView.setInstrument (inst);
        keyboard.setRoots (inst != nullptr ? inst->rootKeys : std::vector<int>());
    }

    if (proc.getSelectedSound() != shownSelection)
        syncToSelection();

    String status;
    if (! proc.getBank().isLoaded()) status = proc.getBankError() + "\nOpen Settings to choose its location.";
    else if (proc.isLoading()) status = "Loading...";
    else if (proc.getLoadError().isNotEmpty()) status = proc.getLoadError();
    hero.setStatus (status);

    repaint (topBar.withTrimmedLeft (topBar.getWidth() - 300));
}

static void drawWood (Graphics& g, Rectangle<float> r)
{
    g.setGradientFill (ColourGradient (Colour (0xff5a2f17), r.getX(), r.getY(), Colour (0xff2b140a), r.getRight(), r.getY(), false));
    g.fillRect (r);
    Random rnd (11);
    for (int i = 0; i < 26; ++i)
    {
        Path grain;
        const float x0 = r.getX() + r.getWidth() * rnd.nextFloat();
        grain.startNewSubPath (x0, r.getY());
        for (int k = 1; k <= 20; ++k)
            grain.lineTo (x0 + std::sin (k * 0.7f + i) * (1.0f + 2.0f * rnd.nextFloat()), r.getY() + r.getHeight() * k / 20.0f);
        g.setColour ((i % 3 ? Colour (0xff1c0c05) : Colour (0xff8a5532)).withAlpha (0.18f + 0.15f * rnd.nextFloat()));
        g.strokePath (grain, PathStrokeType (0.6f + rnd.nextFloat()));
    }
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawVerticalLine ((int) r.getRight() - 1, r.getY(), r.getBottom());
    g.drawVerticalLine ((int) r.getX(), r.getY(), r.getBottom());
}

void SwellContent::paint (Graphics& g)
{
    // Brushed dark chassis
    g.setGradientFill (ColourGradient (Colour (0xff1a1a1c), 0, 0, Colour (0xff0c0c0d), 0, (float) getHeight(), false));
    g.fillAll();
    drawWood (g, { 0.0f, 0.0f, 18.0f, (float) getHeight() });
    drawWood (g, { (float) getWidth() - 18.0f, 0.0f, 18.0f, (float) getHeight() });

    // Top bar: Swell mark + wordmark
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRect (18, topBar.getBottom(), getWidth() - 36, 1);
    g.setColour (text);
    g.drawImage (logo, Rectangle<float> (34.0f, 14.0f, 42.0f, 38.0f), RectanglePlacement::centred, true);
    g.setFont (serifFont (34.0f));
    g.drawText ("Swell", 84, 6, 180, 40, Justification::centredLeft);
    g.setColour (dim);
    g.setFont (swellFont (10.5f).withExtraKerningFactor (0.42f));
    g.drawText ("NATURAL SYNTHESIZER", 86, 42, 220, 14, Justification::centredLeft);

    // Tab bar housing
    {
        auto tb = Rectangle<int> (tabs[0].getX(), tabs[0].getY(), tabs[2].getRight() - tabs[0].getX(), tabs[0].getHeight()).toFloat().expanded (4.0f);
        drawWell (g, tb, 8.0f);
    }

    // Status
    g.setColour (faint.brighter (0.5f));
    g.setFont (swellFont (12.0f));
    const int statusX = settingsButton.getX() - 260, statusW = 246;
    g.drawText ("Voices " + String (proc.getActiveVoices()) + "/64   CPU " + String (roundToInt (proc.getCpuLoad() * 100.0f)) + "%",
                statusX, 25, statusW, 17, Justification::centredRight);
    g.setColour (faint.brighter (0.3f));
    g.setFont (swellFont (11.0f));
    g.drawText (String ("v") + JucePlugin_VersionString + utf8 ("  \xc2\xb7  by @sevyondatracks"),
                statusX, 42, statusW, 16, Justification::centredRight);

    if (page == soundsPage)
    {
        // Sound list panel
        drawHardwarePanel (g, listArea.toFloat());
        auto head = listArea.reduced (16, 0).withHeight (42);
        if (! searchVisible)
        {
            String title;
            if (currentCategory == CategoryList::favourites) title = "Favorites";
            else if (currentCategory == CategoryList::all) title = "All Sounds";
            else if (currentCategory >= 0 && currentCategory < (int) proc.getBank().categories.size())
                title = proc.getBank().categories[(size_t) currentCategory].name;
            drawCapsLabel (g, title, head, Justification::centredLeft, 16.0f);
        }
        g.setColour (dim);
        g.setFont (swellFont (13.0f));
        g.drawText (String ((int) visibleSounds.size()) + " Sounds", head.withTrimmedRight (40), Justification::centredRight);
        if (visibleSounds.empty())
        {
            g.setColour (faint.brighter (0.4f));
            g.setFont (swellFont (13.0f));
            const String msg = searchBox.getText().isNotEmpty() ? String ("No sound found")
                             : currentCategory == CategoryList::favourites ? String ("Click the star of a sound\nto add it here") : String();
            g.drawFittedText (msg, listArea.reduced (24, 70), Justification::centredTop, 3);
        }

        drawHardwarePanel (g, macroArea.toFloat());

        drawHardwarePanel (g, envArea.toFloat());
        drawHardwarePanel (g, playArea.toFloat());
        drawCapsLabel (g, "Play", playArea.reduced (16, 0).withHeight (38), Justification::centredLeft, 15.0f);
        drawHardwarePanel (g, fxArea.toFloat());
        drawCapsLabel (g, "Effects", fxArea.reduced (16, 0).withHeight (38), Justification::centredLeft, 15.0f);

        // Octave readout
        g.setColour (currentOctave == 0 ? text : A().accentSoft);
        g.setFont (swellFont (15.0f, true));
        g.drawText ((currentOctave > 0 ? "+" : "") + String (currentOctave), octLabel.withTrimmedBottom (12), Justification::centred);
        g.setColour (faint.brighter (0.4f));
        g.setFont (swellFont (9.5f).withExtraKerningFactor (0.2f));
        g.drawText ("OCTAVE", octLabel.withTrimmedTop (20), Justification::centred);
    }
    else if (page == editPage)
    {
        drawHardwarePanel (g, editRightArea.toFloat());
        auto r = editRightArea.reduced (18, 0);
        drawCapsLabel (g, "Sample", r.withHeight (40), Justification::centredLeft, 15.0f);
        drawCapsLabel (g, "Envelope", r.withY (editRightArea.getY() + 168).withHeight (30), Justification::centredLeft, 15.0f);
        drawCapsLabel (g, "Filter", r.withY (editRightArea.getY() + 330).withHeight (30), Justification::centredLeft, 15.0f);
    }
    else
    {
        const int w = fxPageArea.getWidth() / 3;
        const char* names[] = { "Drive", "Chorus", "Reverb" };
        const char* notes[] = { "Warm saturation", "Stereo width and movement", "Room and space" };
        for (int i = 0; i < 3; ++i)
        {
            auto pr = Rectangle<int> (fxPageArea.getX() + i * w, fxPageArea.getY(), w, fxPageArea.getHeight()).reduced (i == 0 ? 0 : 5, 0).withTrimmedRight (i == 2 ? 0 : 5);
            drawHardwarePanel (g, pr.toFloat());
            drawCapsLabel (g, names[i], pr.reduced (22, 0).withHeight (52), Justification::centredLeft, 18.0f);
            g.setColour (dim);
            g.setFont (swellFont (13.0f));
            g.drawText (notes[i], pr.reduced (22, 0).withY (pr.getY() + 44).withHeight (20), Justification::centredLeft);
        }
    }

    // Keyboard block
    drawHardwarePanel (g, wheelArea.toFloat());
    drawHardwarePanel (g, keysBlock.toFloat());
}

void SwellContent::resized()
{
    const int L = 30, R = baseW - 30;
    topBar = { 18, 0, baseW - 36, 66 };
    for (int i = 0; i < 3; ++i)
        tabs[i].setBounds (430 + i * 142, 14, 138, 38);
    settingsButton.setBounds (R - 46, 13, 46, 40);
    // Colonne d'état à gauche de la roue : licence, voix/CPU, version (trois lignes alignées à droite).
    licenceBadge.setBounds (settingsButton.getX() - 260, 8, 246, 17);
    licenceBadge.setFontHeight (12.0f);

    // ---------- Sounds page
    const int y1 = 76, row1H = 330, y2 = y1 + row1H + 10, row2H = 178;
    libArea = { L, y1, 236, row1H };
    categoryList.setBounds (libArea);
    listArea = { libArea.getRight() + 10, y1, 316, row1H };
    {
        auto head = listArea.reduced (12, 0).withHeight (42);
        searchButton.setBounds (head.removeFromRight (32).withSizeKeepingCentre (32, 30));
        searchBox.setBounds (head.withTrimmedRight (110).withSizeKeepingCentre (head.getWidth() - 110, 30).withX (listArea.getX() + 12));
        soundList.setBounds (listArea.reduced (8, 0).withTrimmedTop (44).withTrimmedBottom (8));
    }
    const int hx = listArea.getRight() + 10;
    heroArea = { hx, y1, R - hx, 200 };
    hero.setBounds (heroArea);
    prevButton.setBounds (heroArea.getX() + 8, heroArea.getCentreY() + 10, 34, 50);
    nextButton.setBounds (heroArea.getRight() - 42, heroArea.getCentreY() + 10, 34, 50);
    macroArea = { hx, heroArea.getBottom() + 10, R - hx, row1H - 210 };
    {
        auto m = macroArea.reduced (20, 10);
        const int w = m.getWidth() / macroKnobs.size();
        for (auto* k : macroKnobs) k->setBounds (m.removeFromLeft (w).reduced (12, 0));
    }

    envArea = { L, y2, listArea.getRight() - L, row2H };
    {
        auto e = envArea.reduced (12, 10);
        auto tabRow = e.removeFromTop (30);
        for (int i = 0; i < 2; ++i) envTabs[i].setBounds (tabRow.removeFromLeft (110).reduced (2, 0));
        e.removeFromTop (10);
        auto left = e.removeFromLeft (250);
        const int fh = left.getHeight() / 4;
        for (int i = 0; i < ampFaders.size(); ++i) ampFaders[i]->setBounds (left.getX(), left.getY() + i * fh, left.getWidth(), fh);
        for (int i = 0; i < filterFaders.size(); ++i) filterFaders[i]->setBounds (left.getX(), left.getY() + (i + 1) * fh, left.getWidth(), fh);
        e.removeFromLeft (10);
        envGraph.setBounds (e);
    }
    playArea = { envArea.getRight() + 10, y2, 330, row2H };
    {
        auto p = playArea.reduced (16, 0).withTrimmedTop (40).withTrimmedBottom (14);
        auto knobArea = p.removeFromRight (100);
        playKnobs[0]->setBounds (knobArea.withSizeKeepingCentre (96, 108));
        p.removeFromRight (8);
        auto row = p.removeFromTop (32);
        const int bw = row.getWidth() / 3;
        for (auto& b : modeButtons) b.setBounds (row.removeFromLeft (bw).reduced (2, 0));
        p.removeFromTop (14);
        slideArtToggle.setBounds (p.removeFromTop (28));
        p.removeFromTop (10);
        auto oct = p.removeFromTop (36);
        octDown.setBounds (oct.removeFromLeft (34));
        octLabel = oct.removeFromLeft (70);
        octUp.setBounds (oct.removeFromLeft (34));
    }
    fxArea = { playArea.getRight() + 10, y2, R - playArea.getRight() - 10, row2H };
    {
        auto f = fxArea.reduced (14, 0).withTrimmedTop (38).withTrimmedBottom (12);
        const int w = f.getWidth() / fxKnobs.size();
        for (auto* k : fxKnobs) k->setBounds (f.removeFromLeft (w).reduced (6, 0));
    }

    // ---------- Edit page
    const int pageBottom = y2 + row2H;
    editSampleArea = { L, y1, 780, pageBottom - y1 };
    sampleView.setBounds (editSampleArea);
    editRightArea = { editSampleArea.getRight() + 10, y1, R - editSampleArea.getRight() - 10, pageBottom - y1 };
    {
        auto r = editRightArea.reduced (14, 0);
        auto row = [&] (int y, int first, int count)
        {
            auto rr = Rectangle<int> (r.getX(), editRightArea.getY() + y, r.getWidth(), 120);
            const int w = rr.getWidth() / jmax (3, count);
            for (int i = 0; i < count; ++i) editKnobs[first + i]->setBounds (rr.removeFromLeft (w).reduced (8, 0));
        };
        row (40, 0, 3);
        row (196, 3, 4);
        row (360, 7, 2);
    }

    // ---------- Effects page
    fxPageArea = { L, y1, R - L, pageBottom - y1 };
    {
        const int w = fxPageArea.getWidth() / 3;
        for (int i = 0; i < 3; ++i)
            bigFxKnobs[i]->setBounds (Rectangle<int> (fxPageArea.getX() + i * w, fxPageArea.getY() + 80, w, fxPageArea.getHeight() - 100).withSizeKeepingCentre (170, 205));
    }

    // ---------- Keyboard section
    const int ky = pageBottom + 10;
    wheelArea = { L, ky, 170, baseH - ky - 10 };
    {
        auto w = wheelArea.reduced (8, 10);
        const int kw = w.getWidth() / 2;
        outKnobs[0]->setBounds (w.removeFromLeft (kw));
        outKnobs[1]->setBounds (w);
    }
    keysBlock = { wheelArea.getRight() + 10, ky, R - wheelArea.getRight() - 10, baseH - ky - 10 };
    auto kb = keysBlock.reduced (8, 8);
    keyboard.setBounds (kb);
    keyboard.setKeyWidth (kb.getWidth() / 43.0f); // 24..96 = 43 white keys
}

// ================================================================== Editor

static swell::LicencePanel::Style licenceStyle (int theme)
{
    SwellTheme::currentIndex = jlimit (0, SwellTheme::numThemes - 1, theme);
    return { Colour (0xff0c0c0d), SwellTheme::panelTop, SwellTheme::text, SwellTheme::dim, A().accent, Colour (0xffff6b5b) };
}

SwellEditor::SwellEditor (SwellProcessor& p)
    : AudioProcessorEditor (p), proc (p), content (p, *this), settings (*this), licenceGuard (*this, licenceStyle (p.getTheme()))
{
    SwellTheme::currentIndex = jlimit (0, SwellTheme::numThemes - 1, p.getTheme());
    lnf.applyTheme();
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addChildComponent (settings);
    settings.setBounds (0, 0, SwellContent::baseW, SwellContent::baseH);

    const float s = jlimit (0.6f, 2.0f, p.getUiScale());
    constrainer.setFixedAspectRatio ((double) SwellContent::baseW / SwellContent::baseH);
    constrainer.setSizeLimits (roundToInt (SwellContent::baseW * 0.6f), roundToInt (SwellContent::baseH * 0.6f),
                               SwellContent::baseW * 2, SwellContent::baseH * 2);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (roundToInt (SwellContent::baseW * s), roundToInt (SwellContent::baseH * s));
    if (! proc.getBank().isLoaded())
        showSettings (true);
}

SwellEditor::~SwellEditor()
{
    setLookAndFeel (nullptr);
}

void SwellEditor::resized()
{
    const float s = (float) getWidth() / (float) SwellContent::baseW;
    content.setTransform (AffineTransform::scale (s));
    content.setBounds (0, 0, SwellContent::baseW, SwellContent::baseH);
    if (std::abs (s - proc.getUiScale()) > 0.005f && isShowing())
        proc.setUiScale (s);
}

void SwellEditor::setScale (float s)
{
    proc.setUiScale (s);
    setSize (roundToInt (SwellContent::baseW * s), roundToInt (SwellContent::baseH * s));
}

void SwellEditor::setTheme (int t)
{
    SwellTheme::currentIndex = jlimit (0, SwellTheme::numThemes - 1, t);
    proc.setTheme (SwellTheme::currentIndex);
    lnf.applyTheme();
    sendLookAndFeelChange();
    content.themeChanged();
    repaint();
}

void SwellEditor::showSettings (bool show)
{
    settings.refresh();
    settings.setVisible (show);
    if (show) settings.toFront (true);
}
