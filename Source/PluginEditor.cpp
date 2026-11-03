#include "PluginEditor.h"

using namespace gc;

namespace
{
constexpr float kPi = juce::MathConstants<float>::pi;
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
float ease (float t) { t = juce::jlimit (0.0f, 1.0f, t); return 1 - std::pow (1 - t, 3.0f); }
juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
juce::Colour pitchColour (int midi) { return ui::screenColour (((midi % 12) + 12) % 12 % 4); }
const char* kPageGlyph[8] = { "N", "G", "C", "R", "D", "S", "T", "M" };
}

// ============================================================================ look: OP-1 encoders
void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    if (s.getProperties().contains ("anim")) pos = (float) s.getProperties()["anim"];
    const int ci = (int) s.getProperties().getWithDefault ("ci", 0);
    const auto cap = ui::encoderColour (ci);
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 6, cx = b.getCentreX(), cy = b.getCentreY();
    const float ang = start + pos * (end - start);

    // value arc, printed on the body
    juce::Path track; track.addCentredArc (cx, cy, r + 4, r + 4, 0, start, end, true);
    g.setColour (ui::keyEdge); g.strokePath (track, juce::PathStrokeType (1.5f));
    juce::Path val; val.addCentredArc (cx, cy, r + 4, r + 4, 0, start, ang, true);
    g.setColour (ui::ink.withAlpha (0.75f)); g.strokePath (val, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // drop shadow + dark base
    g.setColour (juce::Colours::black.withAlpha (0.18f)); g.fillEllipse (cx - r, cy - r + 3, r * 2, r * 2);
    g.setColour (juce::Colour (0xff2a2a2d)); g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
    // coloured cap
    const float cr = r * 0.78f;
    g.setGradientFill (juce::ColourGradient (cap.brighter (0.25f), cx - cr * 0.5f, cy - cr * 0.7f, cap.darker (0.15f), cx + cr * 0.6f, cy + cr, true));
    g.fillEllipse (cx - cr, cy - cr, cr * 2, cr * 2);
    g.setColour (juce::Colours::white.withAlpha (0.25f)); g.drawEllipse (cx - cr + 1, cy - cr + 1, cr * 2 - 2, cr * 2 - 2, 1);
    // indicator notch
    const float nx = cx + std::sin (ang) * cr * 0.68f, ny = cy - std::cos (ang) * cr * 0.68f;
    g.setColour (ci % 4 == 2 ? ui::ink : juce::Colours::white);
    g.fillEllipse (nx - 3, ny - 3, 6, 6);
}

// ============================================================================ key
void Key::tick()
{
    press += ((down ? 1.0f : 0.0f) - press) * 0.45f;
    glowTarget = isLit ? (isLit() ? 1.0f : 0.0f) : glowTarget;
    glow += (glowTarget - glow) * 0.25f;
    repaint();
}

void Key::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    const float depth = 3.0f * (1 - press);
    g.setColour (juce::Colours::black.withAlpha (0.16f)); g.fillRoundedRectangle (r.withTrimmedTop (3), 6);
    g.setColour (ui::keyEdge); g.fillRoundedRectangle (r.withTrimmedTop (depth > 0 ? 2 : 0), 6);
    auto face = r.withTrimmedBottom (depth).translated (0, 3 - depth);
    face = face.withTrimmedBottom (3 - depth >= 0 ? 0 : 0);
    g.setGradientFill (juce::ColourGradient (ui::keyFace, face.getX(), face.getY(), ui::keyFace.darker (0.04f), face.getX(), face.getBottom(), false));
    g.fillRoundedRectangle (face, 6);
    if (glow > 0.01f) { g.setColour (glowColour.withAlpha (0.10f * glow)); g.fillRoundedRectangle (face, 6); }

    // LED
    const float lx = face.getRight() - 10, ly = face.getY() + 9;
    if (glow > 0.01f) { g.setColour (glowColour.withAlpha (0.35f * glow)); g.fillEllipse (lx - 6, ly - 6, 12, 12); }
    g.setColour (glow > 0.01f ? glowColour.interpolatedWith (juce::Colours::white, 0.2f * glow).withAlpha (0.3f + 0.7f * glow) : ui::keyEdge);
    g.fillEllipse (lx - 2.5f, ly - 2.5f, 5, 5);

    g.setColour (ui::ink);
    if (getWidth() < 52)
    {
        g.setFont (font (15.0f, true)); g.drawText (glyph, face.withTrimmedBottom (10), juce::Justification::centred, false);
        g.setColour (ui::inkSoft); g.setFont (font (8.5f)); g.drawText (label, face.reduced (2, 4), juce::Justification::centredBottom, false);
        return;
    }
    if (glyph.isNotEmpty()) { g.setFont (font (face.getHeight() > 60 ? 15.0f : 13.0f, true)); g.drawText (glyph, face.reduced (8, 6), label.isEmpty() ? juce::Justification::centred : juce::Justification::topLeft, false); }
    if (label.isNotEmpty()) { g.setColour (ui::inkSoft); g.setFont (font (10.0f)); g.drawText (label, face.reduced (8, 6), juce::Justification::bottomLeft, true); }
}

// ============================================================================ screen
void Screen::showValue (const juce::String& name, const juce::String& value, juce::Colour c)
{
    popName = name; popValue = value; popColour = c; popT = 1.0f;
}

void Screen::tick (double t)
{
    now = t;
    const float dt = (float) juce::jlimit (0.0, 0.1, last > 0 ? t - last : 0.016);
    last = t;
    pageT = juce::jmin (1.0f, pageT + dt / 0.28f);
    popT = juce::jmax (0.0f, popT - dt / 1.4f);

    // grain events -> particles (+ rings on the SPACE page)
    auto& f = proc.engine.eventFifo;
    int s1, n1, s2, n2; f.prepareToRead (f.getNumReady(), s1, n1, s2, n2);
    const int base = proc.music().baseMidi();
    juce::Random& rnd = juce::Random::getSystemRandom();
    int k = 0;
    auto take = [&] (const GrainEvent& e)
    {
        if (parts.size() < 500)
        {
            const float y = 1.0f - juce::jlimit (0.0f, 1.0f, (float) (e.midi - (base - 2)) / 30.0f);
            parts.push_back ({ e.p, y, (rnd.nextFloat() - 0.5f) * 0.05f, -0.03f - rnd.nextFloat() * 0.05f, 0, e.sizeSec + 0.7f, 2.0f + e.sizeSec * 14.0f, pitchColour (e.midi) });
        }
        if ((k++ % 5) == 0 && rings.size() < 40) rings.push_back ({ 0.02f, 0, 0.8f + proc.param (pDecay) * 0.35f });
    };
    for (int i = 0; i < n1; ++i) take (proc.engine.events[(size_t) (s1 + i)]);
    for (int i = 0; i < n2; ++i) take (proc.engine.events[(size_t) (s2 + i)]);
    f.finishedRead (n1 + n2);

    for (auto& p : parts) { p.life += dt; p.x += p.vx * dt; p.y += p.vy * dt; }
    parts.erase (std::remove_if (parts.begin(), parts.end(), [] (const Particle& p) { return p.life > p.maxLife; }), parts.end());
    for (auto& r : rings) { r.life += dt; r.r += dt * 0.55f / r.maxLife; }
    rings.erase (std::remove_if (rings.begin(), rings.end(), [] (const Ring& r) { return r.life > r.maxLife; }), rings.end());

    for (int pc = 0; pc < 12; ++pc)
    {
        float lv = 0; for (int m = pc; m < 128; m += 12) lv = juce::jmax (lv, proc.engine.noteLevel[(size_t) m].load());
        pcGlow[(size_t) pc] += (lv - pcGlow[(size_t) pc]) * 0.2f;
    }
    const float wow = proc.param (pWow);
    spin = 1.0f + wow * 0.5f * std::sin ((float) t * kTwoPi * proc.param (pWrate)) + (proc.engine.grainsPerSec.load() > 0 ? 0.6f : 0.0f);
    reelAngle += dt * spin * 2.2f;
    const double ppq = proc.uiPpq.load(); const float frac = (float) (ppq - std::floor (ppq));
    pumpViz = 1.0f - proc.param (pPump) * 0.35f * std::exp (-frac * 6.0f);
    repaint();
}

void Screen::paint (juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff1c1c1e)); g.fillRoundedRectangle (full, 10);
    auto r = full.reduced (6);
    g.setColour (ui::screen); g.fillRoundedRectangle (r, 6);
    r = r.reduced (12, 9);

    // header
    auto head = r.removeFromTop (14);
    g.setFont (font (11.0f, true));
    g.setColour (ui::screenColour (page % 4)); g.fillEllipse (head.getX(), head.getCentreY() - 3, 6, 6);
    g.setColour (juce::Colours::white); g.drawText (kPageNames[page], head.withTrimmedLeft (12), juce::Justification::centredLeft);
    g.setColour (juce::Colours::white.withAlpha (0.45f)); g.setFont (font (11.0f));
    const int cp = proc.getCurrentProgram();
    g.drawText (juce::String (kPresets[cp].name).toLowerCase() + (proc.presetDirty.load() ? " *" : ""), head, juce::Justification::centredRight);

    // footer: the 8 parameters of this page as small coloured bars
    auto foot = r.removeFromBottom (34);
    for (int row = 0; row < 2; ++row)
    {
        auto line = foot.removeFromTop (17);
        const float cw = line.getWidth() / 4;
        for (int c = 0; c < 4; ++c)
        {
            const int i = row * 4 + c, pi = page * 8 + i;
            auto cell = line.removeFromLeft (cw).reduced (3, 2);
            auto* prm = proc.apvts.getParameter (kParams[pi].id);
            const float n = prm != nullptr ? prm->getValue() : 0;
            g.setColour (juce::Colours::white.withAlpha (0.4f)); g.setFont (font (9.0f));
            g.drawText (juce::String (kParams[pi].name).toLowerCase(), cell.removeFromTop (9), juce::Justification::centredLeft, true);
            auto bar = cell.withHeight (2.5f).translated (0, 1.5f);
            g.setColour (juce::Colours::white.withAlpha (0.12f)); g.fillRect (bar);
            g.setColour (ui::screenColour (i)); g.fillRect (bar.withWidth (bar.getWidth() * n));
        }
    }
    r.removeFromBottom (4);

    // page visual, with a slide/fade transition
    const float e = ease (pageT);
    if (e < 1.0f)
    {
        juce::Graphics::ScopedSaveState s (g); g.reduceClipRegion (r.toNearestInt());
        drawPage (g, r.translated (-24 * e, 0), lastPage, 1 - e);
    }
    {
        juce::Graphics::ScopedSaveState s (g); g.reduceClipRegion (r.toNearestInt());
        drawPage (g, r.translated (24 * (1 - e), 0), page, e);
    }

    // value pop-up when a knob moves
    if (popT > 0)
    {
        const float a = juce::jmin (1.0f, popT * 2.5f);
        auto box = r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 20, 230.0f), 64);
        g.setColour (juce::Colours::black.withAlpha (0.85f * a)); g.fillRoundedRectangle (box, 8);
        g.setColour (popColour.withAlpha (a)); g.drawRoundedRectangle (box, 8, 1.5f);
        g.setFont (font (10.5f)); g.setColour (juce::Colours::white.withAlpha (0.6f * a));
        g.drawText (popName.toLowerCase(), box.removeFromTop (24).withTrimmedTop (8), juce::Justification::centred);
        g.setFont (font (24.0f, true)); g.setColour (popColour.withAlpha (a));
        g.drawText (popValue, box.withTrimmedBottom (6), juce::Justification::centred);
    }
}

void Screen::drawPage (juce::Graphics& g, juce::Rectangle<float> r, int pg, float alpha)
{
    g.beginTransparencyLayer (alpha);
    switch (pg)
    {
        case 0: drawNote (g, r); break;
        case 1: drawGrain (g, r); break;
        case 2: drawColor (g, r); break;
        case 3: drawRepeat (g, r); break;
        case 4: drawDirt (g, r); break;
        case 5: drawSpace (g, r); break;
        case 6: drawTape (g, r); break;
        default: drawMassive (g, r); break;
    }
    g.endTransparencyLayer();
}

// NOTE: pitch-class clock. Scale notes ringed, sounding notes glow and are joined into the chord shape.
void Screen::drawNote (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto m = proc.music();
    const float cx = r.getX() + r.getWidth() * 0.34f, cy = r.getCentreY() + 2, R = juce::jmin (r.getHeight() * 0.3f, r.getWidth() * 0.24f);
    juce::Path chord; bool first = true;
    for (int i = 0; i < 12; ++i)
    {
        const int pc = (m.key + i) % 12;
        const float a = (float) i / 12 * kTwoPi - kPi / 2;
        const float x = cx + std::cos (a) * R, y = cy + std::sin (a) * R;
        if (pcGlow[(size_t) pc] > 0.15f) { if (first) chord.startNewSubPath (x, y), first = false; else chord.lineTo (x, y); }
    }
    if (! first) { chord.closeSubPath(); g.setColour (ui::orange.withAlpha (0.18f)); g.fillPath (chord); g.setColour (ui::orange); g.strokePath (chord, juce::PathStrokeType (1.5f)); }
    for (int i = 0; i < 12; ++i)
    {
        const int pc = (m.key + i) % 12;
        const float a = (float) i / 12 * kTwoPi - kPi / 2;
        const float x = cx + std::cos (a) * R, y = cy + std::sin (a) * R;
        const bool in = m.inScale (pc + 60);
        const float gl = pcGlow[(size_t) pc];
        const float d = (i == 0 ? 11.0f : 8.0f) + gl * 7 + (gl > 0.1f ? std::sin ((float) now * 6) * 1.2f : 0);
        if (gl > 0.02f) { g.setColour (pitchColour (pc).withAlpha (0.25f * gl)); g.fillEllipse (x - d, y - d, d * 2, d * 2); }
        if (in) { g.setColour (gl > 0.05f ? pitchColour (pc) : juce::Colours::white.withAlpha (i == 0 ? 0.95f : 0.55f)); g.fillEllipse (x - d / 2, y - d / 2, d, d); }
        else { g.setColour (juce::Colours::white.withAlpha (0.18f)); g.fillEllipse (x - 2, y - 2, 4, 4); }
        g.setColour (juce::Colours::white.withAlpha (in ? 0.5f : 0.2f)); g.setFont (font (9.0f));
        g.drawText (kNoteNames[pc], juce::Rectangle<float> (x, y, 0, 0).expanded (14, 8).translated (std::cos (a) * 18, std::sin (a) * 18), juce::Justification::centred);
    }
    auto info = r.withTrimmedLeft (r.getWidth() * 0.66f);
    g.setColour (juce::Colours::white); g.setFont (font (22.0f, true));
    g.drawText (juce::String (kNoteNames[m.key]), info.removeFromTop (info.getHeight() * 0.5f), juce::Justification::bottomLeft);
    g.setFont (font (11.0f)); g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawText (juce::String (kScales[(size_t) m.scale].name).toLowerCase(), info.removeFromTop (16), juce::Justification::centredLeft);
    g.setColour (ui::green); g.drawText (juce::String (kChords[(size_t) m.chord].name).toLowerCase() + "  /  oct " + juce::String (m.oct), info.removeFromTop (16), juce::Justification::centredLeft);
    const int src = (int) proc.param (pSrc);
    g.setColour (ui::blue); g.drawText (src >= kNumSources ? juce::String ("random") : juce::String (kSourceNames[src]).toLowerCase(), info.removeFromTop (16), juce::Justification::centredLeft);
}

// GRAIN: every grain is a particle floating up from where it was read, height = pitch
void Screen::drawGrain (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float pos = proc.param (pPos), spray = proc.param (pSpray);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRect (r.getX() + (pos - spray * 0.5f) * r.getWidth(), r.getY(), spray * r.getWidth(), r.getHeight());
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    const float px = r.getX() + pos * r.getWidth();
    for (float y = r.getY(); y < r.getBottom(); y += 6) g.fillRect (px - 0.5f, y, 1.0f, 3.0f);
    for (auto& p : parts)
    {
        const float t = p.life / p.maxLife, a = (t < 0.15f ? t / 0.15f : 1 - (t - 0.15f) / 0.85f);
        const float x = r.getX() + p.x * r.getWidth(), y = r.getY() + p.y * r.getHeight(), s = p.size * (0.6f + 0.6f * (1 - t));
        g.setColour (p.c.withAlpha (a * 0.9f)); g.fillEllipse (x - s / 2, y - s / 2, s, s);
    }
    g.setColour (juce::Colours::white.withAlpha (0.5f)); g.setFont (font (10.0f));
    g.drawText (juce::String (proc.engine.grainsPerSec.load()) + " grains/s", r, juce::Justification::topRight);
}

// COLOR: a prism. Ray spread = width, ray jitter = detune, dots above/below = octave grains
void Screen::drawColor (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float width = proc.param (pWidth), det = proc.param (pDetune) / 50.0f;
    const float ox = r.getCentreX(), oy = r.getBottom() - 4;
    const float act = juce::jmin (1.0f, proc.engine.grainsPerSec.load() / 40.0f);
    for (int i = 0; i < 14; ++i)
    {
        const float u = (float) i / 13 - 0.5f;
        const float a = -kPi / 2 + u * (0.25f + width * 2.2f) + std::sin ((float) now * 9 + (float) i * 1.7f) * det * 0.06f;
        const float len = r.getHeight() * (0.55f + 0.4f * act) * (0.85f + 0.15f * std::sin ((float) now * 3 + (float) i));
        g.setColour (ui::screenColour (i).withAlpha (0.35f + 0.5f * act));
        g.drawLine (ox, oy, ox + std::cos (a) * len, oy + std::sin (a) * len, 2.0f);
    }
    const float up = proc.param (pOctUp), dn = proc.param (pOctDn);
    for (int i = 0; i < 24; ++i)
    {
        const float x = r.getX() + std::fmod ((float) i * 37.0f + (float) now * 20.0f, r.getWidth());
        if ((float) (i % 10) / 10 < up) { g.setColour (ui::white.withAlpha (0.7f)); g.fillEllipse (x, r.getY() + 4 + std::sin ((float) now * 2 + (float) i) * 3, 3, 3); }
        if ((float) (i % 10) / 10 < dn) { g.setColour (ui::orange.withAlpha (0.7f)); g.fillEllipse (x, r.getBottom() - 10, 3, 3); }
    }
    g.setColour (juce::Colours::white); g.fillEllipse (ox - 5, oy - 5, 10, 10);
}

// REPEAT: 16 steps, playhead follows the beat, lit steps are where note repeat fires
void Screen::drawRepeat (juce::Graphics& g, juce::Rectangle<float> r)
{
    const double ppq = proc.uiPpq.load(); const int rate = (int) proc.param (pRate);
    const int step = (int) std::floor (ppq * 4.0) % 16;
    const float cw = r.getWidth() / 16;
    auto row = r.withSizeKeepingCentre (r.getWidth(), juce::jmin (cw - 4, r.getHeight() * 0.35f)).translated (0, 10);
    static const int every[7] = { 0, 4, 2, 0, 1, 0, 1 };
    for (int i = 0; i < 16; ++i)
    {
        auto c = juce::Rectangle<float> (row.getX() + (float) i * cw, row.getY(), cw, row.getHeight()).reduced (2);
        const bool hit = rate > 0 && (every[rate] == 0 ? (i % (rate == 3 ? 3 : 2) == 0) : i % every[rate] == 0);
        const bool head = i == step;
        g.setColour (head ? ui::orange : (hit ? ui::green.withAlpha (0.85f) : juce::Colours::white.withAlpha (i % 4 == 0 ? 0.22f : 0.1f)));
        g.fillRoundedRectangle (c.reduced (head ? 0.0f : 1.0f), 3);
    }
    g.setColour (juce::Colours::white); g.setFont (font (26.0f, true));
    g.drawText (juce::String (juce::roundToInt (proc.uiBpm.load())), r.withHeight (r.getHeight() * 0.38f), juce::Justification::centredLeft);
    g.setFont (font (10.0f)); g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.drawText ("bpm   " + formatValue (kParams[pRate], (float) rate).toLowerCase() + "   swing " + juce::String (juce::roundToInt (proc.param (pSwing))) + "%",
                r.withHeight (r.getHeight() * 0.38f).withTrimmedLeft (54), juce::Justification::centredLeft);
}

// DIRT: a sine wave being driven into the fuzz and crushed in real time
void Screen::drawDirt (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float drive = proc.param (pDrive), mix = proc.param (pDmix), bits = proc.param (pBits), vow = proc.param (pVowel);
    const float k = 1 + drive * 14 * (0.3f + mix), norm = 1.0f / std::tanh (k);
    const float lv = bits >= 12 ? 0.0f : std::pow (2.0f, bits - 2);
    juce::Path clean, dirty;
    const float cy = r.getCentreY(), A = r.getHeight() * 0.38f;
    for (int x = 0; x <= (int) r.getWidth(); x += 2)
    {
        const float ph = (float) x / r.getWidth() * kTwoPi * 2 + (float) now * 2.5f;
        float s = std::sin (ph) + std::sin (ph * 3 + vow) * 0.15f * vow / 4;
        const float c = s;
        s = std::tanh (k * s) * norm;
        if (lv > 0) s = std::round (s * lv) / lv;
        const float X = r.getX() + (float) x;
        if (x == 0) { clean.startNewSubPath (X, cy - c * A * 0.8f); dirty.startNewSubPath (X, cy - s * A); }
        else { clean.lineTo (X, cy - c * A * 0.8f); dirty.lineTo (X, cy - s * A); }
    }
    g.setColour (juce::Colours::white.withAlpha (0.2f)); g.strokePath (clean, juce::PathStrokeType (1.0f));
    g.setColour (ui::orange.withAlpha (0.3f + 0.7f * juce::jmin (1.0f, mix * 2))); g.strokePath (dirty, juce::PathStrokeType (2.0f + mix * 2));
}

// SPACE: rings bloom from each burst of grains; the whole scene breathes with the pump; gate pattern below
void Screen::drawSpace (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float cx = r.getCentreX(), cy = r.getCentreY() - 6, R = juce::jmin (r.getWidth(), r.getHeight()) * 0.55f * pumpViz;
    for (auto& ring : rings)
    {
        const float t = ring.life / ring.maxLife, rr = ring.r * R * 2.2f;
        g.setColour (ui::blue.interpolatedWith (juce::Colours::white, proc.param (pShimmer) * 0.6f).withAlpha ((1 - t) * 0.8f * (0.3f + proc.param (pGmix))));
        g.drawEllipse (cx - rr, cy - rr * 0.6f, rr * 2, rr * 1.2f, 1.5f);
    }
    const float core = 6 + pumpViz * 4;
    g.setColour (juce::Colours::white); g.fillEllipse (cx - core / 2, cy - core / 2, core, core);
    const int pat = (int) proc.param (pGpat);
    const int step = (int) std::floor (proc.uiPpq.load() * 4.0) % 16;
    auto row = r.removeFromBottom (8);
    const float cw = row.getWidth() / 16;
    for (int i = 0; i < 16; ++i)
    {
        const bool on = kGates[pat][i] == '1';
        g.setColour (i == step ? ui::orange : juce::Colours::white.withAlpha (on ? 0.5f : 0.1f));
        g.fillRect (row.getX() + (float) i * cw + 1, row.getY(), cw - 2, 4.0f);
    }
}

// TAPE: two reels; speed wobbles with wow, the picture warms and darkens with Darken
void Screen::drawTape (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float dark = proc.param (pDarken), hiss = proc.param (pHiss);
    const float R = juce::jmin (r.getHeight() * 0.36f, r.getWidth() * 0.18f);
    const float y = r.getCentreY() - 4;
    const float xs[2] = { r.getX() + r.getWidth() * 0.27f, r.getX() + r.getWidth() * 0.73f };
    const auto ink = juce::Colours::white.interpolatedWith (ui::orange, dark * 0.8f).withMultipliedBrightness (1.0f - dark * 0.35f);
    for (int k = 0; k < 2; ++k)
    {
        const float rad = R * (k == 0 ? 1.0f : 0.78f);
        g.setColour (ink.withAlpha (0.18f)); g.fillEllipse (xs[k] - rad, y - rad, rad * 2, rad * 2);
        g.setColour (ink); g.drawEllipse (xs[k] - rad, y - rad, rad * 2, rad * 2, 1.5f);
        g.drawEllipse (xs[k] - R * 0.28f, y - R * 0.28f, R * 0.56f, R * 0.56f, 1.5f);
        for (int s = 0; s < 3; ++s)
        {
            const float a = reelAngle * (k == 0 ? 1.0f : 1.25f) + (float) s * kTwoPi / 3;
            g.drawLine (xs[k] + std::cos (a) * R * 0.28f, y + std::sin (a) * R * 0.28f, xs[k] + std::cos (a) * rad * 0.8f, y + std::sin (a) * rad * 0.8f, 2.0f);
        }
    }
    g.setColour (ink.withAlpha (0.8f));
    g.drawLine (xs[0], y + R, xs[1], y + R * 0.78f + (proc.param (pWow) * 3 * std::sin ((float) now * 5)), 1.5f);
    juce::Random rnd ((juce::int64) (now * 30));
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    for (int i = 0; i < (int) (hiss * 60); ++i) g.fillRect (r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight(), 1.0f, 1.0f);
    if (rnd.nextFloat() < proc.param (pCrackle) * 0.4f) { g.setColour (juce::Colours::white); g.fillEllipse (r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight(), 3, 3); }
}

// MASSIVE: a slowly turning galaxy. Size = delay, arms = density, trails = feedback, wobble = warp
void Screen::drawMassive (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float size = (proc.param (pMsize) - 0.3f) / 11.7f, fb = proc.param (pMfb), dens = proc.param (pMdens), warp = proc.param (pMwarp), mix = proc.param (pMmix);
    const float cx = r.getCentreX(), cy = r.getCentreY(), R = juce::jmin (r.getWidth(), r.getHeight()) * (0.25f + 0.25f * std::sqrt (size));
    const int arms = 2 + (int) (dens * 3);
    const float rot = (float) now * 0.25f * proc.param (pMrate);
    const int pts = 40 + (int) (fb * 80);
    for (int a = 0; a < arms; ++a)
        for (int i = 0; i < pts; ++i)
        {
            const float t = (float) i / (float) pts;
            const float ang = rot + (float) a * kTwoPi / (float) arms + t * 4.5f + warp * 0.3f * std::sin ((float) now * 2 + t * 9);
            const float rr = R * (0.12f + t);
            const float x = cx + std::cos (ang) * rr, y = cy + std::sin (ang) * rr * 0.55f;
            const auto c = ui::screenColour (a).interpolatedWith (juce::Colours::white, 1 - t);
            g.setColour (c.withAlpha ((1 - t) * (0.35f + 0.65f * mix + 0.2f)));
            const float s = 1.2f + (1 - t) * 2.2f;
            g.fillEllipse (x - s / 2, y - s / 2, s, s);
        }
    g.setColour (juce::Colours::black); g.fillEllipse (cx - R * 0.1f, cy - R * 0.06f, R * 0.2f, R * 0.12f);
    g.setColour (juce::Colours::white.withAlpha (0.8f)); g.drawEllipse (cx - R * 0.1f, cy - R * 0.06f, R * 0.2f, R * 0.12f, 1.0f);
}

// ============================================================================ editor
GrainChopEditor::GrainChopEditor (GrainChopProcessor& p) : AudioProcessorEditor (&p), proc (p), screen (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (screen);

    for (int i = 0; i < 8; ++i)
    {
        auto& k = knobs[(size_t) i];
        k.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.setRotaryParameters (kPi * 1.25f, kPi * 2.75f, true);
        k.setMouseDragSensitivity (220);
        k.getProperties().set ("ci", i);
        k.onValueChange = [this, i]
        {
            if (reattaching) return;
            const int pi = page * 8 + i;
            if (auto* prm = proc.apvts.getParameter (kParams[pi].id))
                screen.showValue (kParams[pi].name, prm->getCurrentValueAsText(), ui::screenColour (i));
        };
        addAndMakeVisible (k);
        for (auto* l : { &knobNames[(size_t) i], &knobValues[(size_t) i] }) { l->setJustificationType (juce::Justification::centred); l->setInterceptsMouseClicks (false, false); addAndMakeVisible (*l); }
        knobNames[(size_t) i].setFont (font (11.5f, true)); knobNames[(size_t) i].setColour (juce::Label::textColourId, ui::ink);
        knobValues[(size_t) i].setFont (font (10.5f)); knobValues[(size_t) i].setColour (juce::Label::textColourId, ui::inkSoft);
    }

    for (int i = 0; i < kNumPages; ++i)
    {
        auto* key = pageKeys.add (new Key());
        key->glyph = kPageGlyph[i]; key->label = juce::String (kPageNames[i]).toLowerCase();
        key->glowColour = ui::screenColour (i);
        key->onPress = [this, i] { setPage (i); };
        key->isLit = [this, i] { return page == i; };
        addAndMakeVisible (key);
    }

    auto addFunc = [this] (const char* glyph, const char* label, std::function<void()> press, std::function<bool()> lit = {}, juce::Colour c = ui::orange)
    {
        auto* key = funcKeys.add (new Key());
        key->glyph = glyph; key->label = label; key->onPress = std::move (press); key->isLit = std::move (lit); key->glowColour = c;
        addAndMakeVisible (key);
    };
    addFunc ("<", "prev", [this] { stepPreset (-1); });
    addFunc (">", "next", [this] { stepPreset (1); });
    addFunc ("-", "oct", [this] { proc.setParam (pOct, juce::jmax (1.0f, proc.param (pOct) - 1)); });
    addFunc ("+", "oct", [this] { proc.setParam (pOct, juce::jmin (5.0f, proc.param (pOct) + 1)); });
    addFunc ("H", "hold", [this] { proc.setParam (pHold, proc.param (pHold) > 0.5f ? 0.0f : 1.0f); }, [this] { return proc.param (pHold) > 0.5f; }, ui::orange);
    addFunc ("P", "pads", [this] { proc.setParam (pPerPad, proc.param (pPerPad) > 0.5f ? 0.0f : 1.0f); }, [this] { return proc.param (pPerPad) > 0.5f; }, ui::blue);
    addFunc ("?", "shuf", [this] { proc.randomizePadSources(); });
    addFunc ("x", "stop", [this] { proc.stopAll(); });

    for (int i = 0; i < 16; ++i)
    {
        auto* key = pads.add (new Key());
        key->onPress = [this, i]
        {
            auto m = juce::MidiMessage::noteOn (16, 36 + i, (juce::uint8) 108);
            m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001); proc.uiMidi.addMessageToQueue (m);
        };
        key->onRelease = [this, i]
        {
            auto m = juce::MidiMessage::noteOff (16, 36 + i);
            m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001); proc.uiMidi.addMessageToQueue (m);
        };
        addAndMakeVisible (key);
    }

    setPage (0);
    setSize (880, 520);
    startTimerHz (60);
}

GrainChopEditor::~GrainChopEditor() { stopTimer(); for (auto& k : knobs) k.setLookAndFeel (nullptr); setLookAndFeel (nullptr); }

void GrainChopEditor::stepPreset (int d)
{
    const int idx = (proc.getCurrentProgram() + d + kNumPresets) % kNumPresets;
    proc.setCurrentProgram (idx);
    proc.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));
    screen.showValue ("preset " + juce::String (idx + 1), kPresets[idx].name, ui::orange);
}

void GrainChopEditor::setPage (int pg)
{
    page = pg;
    reattaching = true;
    for (int i = 0; i < 8; ++i)
    {
        const int pi = pg * 8 + i;
        knobAtt[(size_t) i].reset();
        knobAtt[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, kParams[pi].id, knobs[(size_t) i]);
        knobs[(size_t) i].setDoubleClickReturnValue (true, kParams[pi].def);
        knobs[(size_t) i].setTooltip (kParams[pi].name);
        knobNames[(size_t) i].setText (juce::String (kParams[pi].name).toLowerCase(), juce::dontSendNotification);
    }
    reattaching = false;
    screen.setPage (pg);
}

void GrainChopEditor::timerCallback()
{
    screen.tick (juce::Time::getMillisecondCounterHiRes() * 0.001);

    // knobs glide to their value
    for (int i = 0; i < 8; ++i)
    {
        auto& k = knobs[(size_t) i];
        const float target = (float) k.valueToProportionOfLength (k.getValue());
        knobAnim[(size_t) i] += (target - knobAnim[(size_t) i]) * 0.3f;
        if (std::abs (target - knobAnim[(size_t) i]) > 0.0005f || ! k.getProperties().contains ("anim")) { k.getProperties().set ("anim", knobAnim[(size_t) i]); k.repaint(); }
    }

    for (int i = 0; i < 8; ++i)
        if (auto* prm = proc.apvts.getParameter (kParams[page * 8 + i].id))
            knobValues[(size_t) i].setText (prm->getCurrentValueAsText(), juce::dontSendNotification);
    for (auto* k : pageKeys) k->tick();
    for (auto* k : funcKeys) k->tick();

    const auto m = proc.music();
    const bool perPad = proc.param (pPerPad) > 0.5f;
    const int last = proc.lastPad.load();
    const int nsteps = (int) m.steps().size();
    for (int i = 0; i < 16; ++i)
    {
        auto* pd = pads[i];
        const int root = m.degMidi (i);
        const int s = proc.padSrc[(size_t) i].load();
        pd->glyph = m.padLabel (i);
        pd->label = perPad ? (s >= kNumSources ? juce::String ("random") : juce::String (kSourceNames[s]).toLowerCase())
                           : (i % nsteps == 0 ? juce::String ("root") : juce::String ("deg ") + juce::String (i % nsteps + 1));
        pd->glowColour = perPad && i == last ? ui::blue : pitchColour (root);
        pd->glowTarget = juce::jmax (juce::jmin (1.0f, proc.engine.noteLevel[(size_t) juce::jlimit (0, 127, root)].load() * 1.2f), perPad && i == last ? 0.35f : 0.0f);
        pd->tick();
    }
}

void GrainChopEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (ui::bodyTop, 0, 0, ui::bodyBot, 0, r.getHeight(), false));
    g.fillAll();
    g.setColour (juce::Colours::white.withAlpha (0.5f)); g.drawHorizontalLine (0, 0, r.getWidth());
    // engraved details
    g.setColour (ui::inkSoft); g.setFont (font (10.0f));
    g.drawText ("grainchop", r.reduced (24, 6).removeFromBottom (12), juce::Justification::bottomRight);
    g.setColour (ui::keyEdge);
    for (int i = 0; i < 6; ++i) g.fillEllipse (24.0f + (float) i * 7, r.getHeight() - 12, 3, 3);
}

void GrainChopEditor::resized()
{
    auto r = getLocalBounds().reduced (22);
    auto top = r.removeFromTop (214);
    screen.setBounds (top.removeFromLeft (460));
    top.removeFromLeft (24);
    const int cw = top.getWidth() / 4, ch = top.getHeight() / 2;
    for (int i = 0; i < 8; ++i)
    {
        auto cell = juce::Rectangle<int> (top.getX() + (i % 4) * cw, top.getY() + (i / 4) * ch, cw, ch);
        knobValues[(size_t) i].setBounds (cell.removeFromBottom (13));
        knobNames[(size_t) i].setBounds (cell.removeFromBottom (14));
        knobs[(size_t) i].setBounds (cell);
    }

    r.removeFromTop (16);
    auto mid = r.removeFromTop (46);
    auto pk = mid.removeFromLeft (460);
    const float pw = (float) pk.getWidth() / 8;
    for (int i = 0; i < 8; ++i) pageKeys[i]->setBounds (juce::Rectangle<float> ((float) pk.getX() + pw * (float) i, (float) pk.getY(), pw, (float) pk.getHeight()).reduced (3, 0).toNearestInt());
    mid.removeFromLeft (24);
    const float fw = (float) mid.getWidth() / 8;
    for (int i = 0; i < 8; ++i) funcKeys[i]->setBounds (juce::Rectangle<float> ((float) mid.getX() + fw * (float) i, (float) mid.getY(), fw, (float) mid.getHeight()).reduced (3, 0).toNearestInt());

    r.removeFromTop (14);
    r.removeFromBottom (8);
    const float kw = (float) r.getWidth() / 8, kh = (float) r.getHeight() / 2;
    for (int i = 0; i < 16; ++i)
    {
        const int col = i % 8, row = i < 8 ? 1 : 0; // low notes on the bottom row
        pads[i]->setBounds (juce::Rectangle<float> ((float) r.getX() + kw * (float) col, (float) r.getY() + kh * (float) row, kw, kh).reduced (3).toNearestInt());
    }
}
