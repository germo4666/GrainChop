#include "PluginEditor.h"

using namespace gc;

static juce::Colour hueFor (int midi, float a = 1.0f) { return juce::Colour::fromHSV ((float) (((midi % 12) + 12) % 12) / 12.0f, 0.75f, 0.98f, a); }

// ============================================================================ look
GCLook::GCLook()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::body);
    setColour (juce::ComboBox::backgroundColourId, col::key);
    setColour (juce::ComboBox::textColourId, col::fg);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff0e0f10));
    setColour (juce::ComboBox::arrowColourId, col::mute);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::fg);
    setColour (juce::PopupMenu::headerTextColourId, col::mute);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::edge);
    setColour (juce::TextButton::buttonColourId, col::key);
    setColour (juce::TextButton::textColourOffId, col::fg);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::Label::textColourId, col::fg);
    setColour (juce::Slider::trackColourId, col::mute);
    setColour (juce::Slider::backgroundColourId, col::key);
    setColour (juce::Slider::thumbColourId, col::fg);
}

void GCLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) / 2, cx = b.getCentreX(), cy = b.getCentreY();
    const auto acc = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float ang = start + pos * (end - start);
    juce::Path track; track.addCentredArc (cx, cy, r - 2, r - 2, 0, start, end, true);
    g.setColour (col::edge); g.strokePath (track, juce::PathStrokeType (3, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path val; val.addCentredArc (cx, cy, r - 2, r - 2, 0, start, ang, true);
    g.setColour (acc); g.strokePath (val, juce::PathStrokeType (3, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float kr = r - 8;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3e42), cx - kr * 0.4f, cy - kr * 0.5f, juce::Colour (0xff1a1c1e), cx + kr, cy + kr, true));
    g.fillEllipse (cx - kr, cy - kr, kr * 2, kr * 2);
    g.setColour (juce::Colour (0xff0d0e0f)); g.drawEllipse (cx - kr, cy - kr, kr * 2, kr * 2, 1);
    juce::Path ptr; ptr.addRoundedRectangle (-1.5f, -kr + 3, 3, kr * 0.45f, 1.5f);
    g.setColour (acc); g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (cx, cy));
}

void GCLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto acc = b.findColour (juce::TextButton::buttonOnColourId);
    g.setColour (on ? acc.interpolatedWith (col::key, 0.75f) : (down ? col::edge : (over ? col::key.brighter (0.08f) : col::key)));
    g.fillRoundedRectangle (r, 5);
    g.setColour (on ? acc : juce::Colour (0xff0e0f10)); g.drawRoundedRectangle (r, 5, 1);
}

void GCLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId)); g.fillRoundedRectangle (r, 5);
    g.setColour (juce::Colour (0xff0e0f10)); g.drawRoundedRectangle (r, 5, 1);
    juce::Path arr; const float ax = (float) w - 14, ay = (float) h / 2;
    arr.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (col::mute); g.fillPath (arr);
}

// ============================================================================ pad
void Pad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1);
    g.setColour (col::key); g.fillRoundedRectangle (r, 7);
    g.setColour (colour.withAlpha (juce::jlimit (0.0f, 1.0f, 0.08f + level * 0.75f + (down ? 0.15f : 0.0f)))); g.fillRoundedRectangle (r, 7);
    g.setColour (current ? col::tape : (down ? col::fg : (isRoot ? colour.withAlpha (0.55f) : juce::Colour (0xff0e0f10))));
    g.drawRoundedRectangle (r, 7, current ? 2.0f : 1.0f);
    g.setColour (col::fg); g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText (label, r.reduced (8, 6).removeFromTop (18), juce::Justification::topLeft, true);
    g.setColour (col::mute); g.setFont (juce::FontOptions (11.0f));
    g.drawText (sub, r.reduced (8, 6).removeFromBottom (15), juce::Justification::bottomLeft, true);
}

void Pad::mouseDown (const juce::MouseEvent&)
{
    down = true; repaint();
    auto m = juce::MidiMessage::noteOn (16, 36 + index, (juce::uint8) 108);
    m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    proc.uiMidi.addMessageToQueue (m);
}

void Pad::mouseUp (const juce::MouseEvent&)
{
    down = false; repaint();
    auto m = juce::MidiMessage::noteOff (16, 36 + index);
    m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    proc.uiMidi.addMessageToQueue (m);
}

// ============================================================================ screen
void Screen::buildOverview (int w)
{
    overview.assign ((size_t) w, { 0.0f, 0.0f });
    const auto& b = proc.engine.src; const int n = b.getNumSamples();
    const float* d = b.getReadPointer (0);
    for (int x = 0; x < w; ++x)
    {
        const int s = (int) ((double) x / w * n), e = (int) ((double) (x + 1) / w * n);
        float lo = 0, hi = 0; for (int i = s; i < e; i += juce::jmax (1, (e - s) / 200)) { lo = juce::jmin (lo, d[i]); hi = juce::jmax (hi, d[i]); }
        overview[(size_t) x] = { lo, hi };
    }
}

void Screen::pull()
{
    auto& f = proc.engine.eventFifo;
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    int s1, n1, s2, n2; const int ready = f.getNumReady();
    f.prepareToRead (ready, s1, n1, s2, n2);
    for (int i = 0; i < n1; ++i) evs.push_back ({ proc.engine.events[(size_t) (s1 + i)], now });
    for (int i = 0; i < n2; ++i) evs.push_back ({ proc.engine.events[(size_t) (s2 + i)], now });
    f.finishedRead (n1 + n2);
    evs.erase (std::remove_if (evs.begin(), evs.end(), [now] (const Ev& e) { return now - e.t > e.e.sizeSec + 0.3; }), evs.end());
    if (evs.size() > 1200) evs.erase (evs.begin(), evs.begin() + (long) (evs.size() - 1200));
}

void Screen::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::screen); g.fillRoundedRectangle (r, 6);
    g.setColour (juce::Colour (0xff1f2224)); g.drawRoundedRectangle (r.reduced (0.5f), 6, 1);
    r = r.reduced (8);
    const int W = (int) r.getWidth();
    if ((int) overview.size() != W) buildOverview (W);
    const double total = proc.engine.src.getNumSamples() / proc.engine.srcRate;
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const int srcSel = juce::jmin (kNumSources - 1, (int) proc.param (pSrc));
    const bool randomSrc = (int) proc.param (pSrc) >= kNumSources;

    // overview strip
    auto top = r.removeFromTop (22);
    for (int i = 0; i < kNumSources; ++i)
    {
        const float x0 = top.getX() + (float) (sliceStart (i) / total) * top.getWidth(), x1 = top.getX() + (float) (sliceEnd (i) / total) * top.getWidth();
        g.setColour ((i == srcSel && ! randomSrc) ? juce::Colour (0xff1a1f20) : (i % 2 ? juce::Colour (0xff0c0e0f) : juce::Colour (0xff090a0b)));
        g.fillRect (x0, top.getY(), x1 - x0, top.getHeight());
    }
    g.setColour (col::dim);
    const float oc = top.getCentreY();
    for (int x = 0; x < W; ++x) { auto [lo, hi] = overview[(size_t) x]; g.fillRect (top.getX() + (float) x, oc - hi * 10, 1.0f, juce::jmax (1.0f, (hi - lo) * 10)); }
    for (auto& e : evs)
    {
        const double age = now - e.t; if (age > e.e.sizeSec + 0.15) continue;
        g.setColour (hueFor (e.e.midi, (float) (1.0 - age / (e.e.sizeSec + 0.15))));
        g.fillRect (top.getX() + (float) (e.e.absSec / total) * top.getWidth() - 1, top.getY() + 2, 2.0f, top.getHeight() - 4);
    }
    r.removeFromTop (6);

    // zoom of the current source with grains layered by pitch
    const auto& b = proc.engine.src; const float* d = b.getReadPointer (0);
    const double s0 = sliceStart (srcSel), dur = sliceEnd (srcSel) - s0, sr = proc.engine.srcRate;
    const int a0 = (int) (s0 * sr), ns = (int) (dur * sr);
    const float mc = r.getCentreY(), mh = r.getHeight();
    g.setColour (juce::Colour (0xff15181a));
    for (int q = 1; q < 4; ++q) g.fillRect (r.getX() + r.getWidth() * (float) q / 4, r.getY(), 1.0f, mh);
    g.setColour (juce::Colour (0xff2f3634));
    for (int x = 0; x < W; ++x)
    {
        const int s = a0 + (int) ((double) x / W * ns), e = a0 + (int) ((double) (x + 1) / W * ns);
        float lo = 1, hi = -1; for (int i = s; i < e && i < b.getNumSamples(); i += juce::jmax (1, (e - s) / 100)) { lo = juce::jmin (lo, d[i]); hi = juce::jmax (hi, d[i]); }
        if (hi >= lo) g.fillRect (r.getX() + (float) x, mc - hi * mh * 0.46f, 1.0f, juce::jmax (1.0f, (hi - lo) * mh * 0.46f));
    }
    const int base = proc.music().baseMidi(), lo = base - 2, hi = base + 28;
    for (auto& e : evs)
    {
        const double age = now - e.t, sz = e.e.sizeSec;
        if (age > sz + 0.25) continue;
        const double life = age / sz;
        const float a = (float) (life < 1 ? std::sin (juce::MathConstants<double>::pi * life) * 0.9 + 0.1 : juce::jmax (0.0, 1 - (age - sz) / 0.25) * 0.25);
        const float x = r.getX() + e.e.p * r.getWidth(), w = juce::jmax (3.0f, e.e.width * r.getWidth());
        const float y = r.getY() + 8 + (1 - juce::jlimit (0.0f, 1.0f, (float) (e.e.midi - lo) / (float) (hi - lo))) * (mh - 16);
        g.setColour (hueFor (e.e.midi, a)); g.fillRect (e.e.rev ? x - w : x, y - 2, w, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (a)); g.fillRect (x - 0.5f, y - 5, 1.0f, 10.0f);
    }

    // labels
    juce::String held;
    for (int m = 0; m < 128; ++m) if (proc.engine.noteLevel[(size_t) m].load() > 0.5f) held << Music::noteName (m) << " ";
    const auto mu = proc.music();
    juce::String lab = presetLabel + "   " + (randomSrc ? juce::String ("RANDOM") : juce::String (kSourceNames[srcSel]).toUpperCase())
                       + "   " + kNoteNames[mu.key] + " " + juce::String (kScales[(size_t) mu.scale].name).toUpperCase() + (held.isNotEmpty() ? "   " + held : juce::String());
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.setColour (juce::Colour (0xc0040505)); g.fillRect (r.getX(), r.getY(), juce::jmin (r.getWidth(), (float) lab.length() * 7.4f + 12), 18.0f);
    g.setColour (col::fg); g.drawText (lab, r.withHeight (18).withTrimmedLeft (6), juce::Justification::centredLeft, true);
    const double effMs = juce::jlimit (8.0, 700.0, (double) proc.param (pSize) * std::pow (28.0 / proc.param (pGrains), 0.6 * proc.param (pLink)));
    g.setColour (juce::Colour (kPageColours[pageColour])); g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String (proc.engine.grainsPerSec.load()) + " GRAINS/S  ~" + juce::String (juce::roundToInt (effMs)) + " MS",
                r.removeFromBottom (16).withTrimmedRight (4), juce::Justification::centredRight);
}

// ============================================================================ editor
static const int kMacroParams[7] = { pGrains, pSize, pDmix, pGmix, pWow, pDarken, pMmix };
static const char* kMacroNames[7] = { "Grains", "Length", "Dirt", "Glass", "Wow", "Darken", "Massive" };

GrainChopEditor::GrainChopEditor (GrainChopProcessor& p) : AudioProcessorEditor (&p), proc (p), screen (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (screen);

    for (int i = 0; i < kNumPages; ++i)
    {
        auto* t = tabs.add (new juce::TextButton (kPageNames[i]));
        t->setClickingTogglesState (false);
        t->setColour (juce::TextButton::buttonOnColourId, juce::Colour (kPageColours[i]));
        t->onClick = [this, i] { setPage (i); };
        addAndMakeVisible (t);
    }
    for (int i = 0; i < 8; ++i)
    {
        knobs[(size_t) i].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knobs[(size_t) i].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knobs[(size_t) i].setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible (knobs[(size_t) i]);
        for (auto* l : { &knobNames[(size_t) i], &knobValues[(size_t) i] }) { l->setJustificationType (juce::Justification::centred); addAndMakeVisible (*l); }
        knobNames[(size_t) i].setFont (juce::FontOptions (11.0f)); knobNames[(size_t) i].setColour (juce::Label::textColourId, col::mute);
        knobValues[(size_t) i].setFont (juce::FontOptions (13.0f, juce::Font::bold));
    }
    for (int i = 0; i < 16; ++i) addAndMakeVisible (pads.add (new Pad (proc, i)));

    // presets
    {
        juce::String cat; int id = 1;
        for (int i = 0; i < kNumPresets; ++i)
        {
            if (cat != kPresets[i].category) { cat = kPresets[i].category; presetBox.addSectionHeading (cat); }
            presetBox.addItem (juce::String (i + 1).paddedLeft ('0', 2) + "  " + kPresets[i].name, id++);
        }
        presetBox.onChange = [this]
        {
            const int idx = presetBox.getSelectedId() - 1;
            if (idx >= 0 && idx != proc.getCurrentProgram()) { proc.setCurrentProgram (idx); proc.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true)); }
        };
        auto step = [this] (int d) { const int idx = (proc.getCurrentProgram() + d + kNumPresets) % kNumPresets; proc.setCurrentProgram (idx); proc.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true)); };
        prevPreset.onClick = [step] { step (-1); }; nextPreset.onClick = [step] { step (1); };
        for (auto* c : { (juce::Component*) &presetBox, (juce::Component*) &prevPreset, (juce::Component*) &nextPreset }) addAndMakeVisible (c);
    }

    auto addCombo = [this] (juce::ComboBox& box, int param)
    {
        auto items = juce::StringArray::fromTokens (kParams[param].choices, "|", "");
        box.addItemList (items, 1);
        comboAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, kParams[param].id, box));
        addAndMakeVisible (box);
    };
    addCombo (keyBox, pKey); addCombo (scaleBox, pScale); addCombo (chordBox, pChord); addCombo (srcBox, pSrc);

    auto addToggle = [this] (juce::TextButton& b, int param, juce::Colour c)
    {
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonOnColourId, c);
        buttonAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, kParams[param].id, b));
        addAndMakeVisible (b);
    };
    addToggle (holdBtn, pHold, juce::Colour (kPageColours[0])); addToggle (perPadBtn, pPerPad, col::tape);
    randomBtn.onClick = [this] { proc.randomizePadSources(); };
    stopBtn.onClick = [this] { proc.stopAll(); };
    octDown.onClick = [this] { proc.setParam (pOct, juce::jmax (1.0f, proc.param (pOct) - 1)); };
    octUp.onClick = [this] { proc.setParam (pOct, juce::jmin (5.0f, proc.param (pOct) + 1)); };
    for (auto* c : { (juce::Component*) &randomBtn, (juce::Component*) &stopBtn, (juce::Component*) &octDown, (juce::Component*) &octUp, (juce::Component*) &octLabel }) addAndMakeVisible (c);
    octLabel.setJustificationType (juce::Justification::centred);

    for (auto [l, t] : { std::pair { &keyHdr, "KEY" }, { &chordHdr, "CHORD PER PAD" }, { &srcHdr, "SOURCE" }, { &macroHdr, "MACROS" }, { &perfHdr, "PERFORMANCE" } })
    {
        l->setText (t, juce::dontSendNotification); l->setFont (juce::FontOptions (10.5f, juce::Font::bold));
        l->setColour (juce::Label::textColourId, col::mute); addAndMakeVisible (*l);
    }

    for (int i = 0; i < kNumMacros; ++i)
    {
        auto& s = macros[(size_t) i];
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, true, 64, 18);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxTextColourId, col::fg);
        macroAtt[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, kParams[kMacroParams[i]].id, s);
        addAndMakeVisible (s);
        macroNames[(size_t) i].setText (kMacroNames[i], juce::dontSendNotification);
        macroNames[(size_t) i].setFont (juce::FontOptions (11.5f)); macroNames[(size_t) i].setColour (juce::Label::textColourId, col::mute);
        addAndMakeVisible (macroNames[(size_t) i]);
    }

    setPage (0);
    setSize (1000, 712);
    startTimerHz (30);
}

GrainChopEditor::~GrainChopEditor() { stopTimer(); setLookAndFeel (nullptr); }

void GrainChopEditor::setPage (int pg)
{
    page = pg;
    const auto c = juce::Colour (kPageColours[pg]);
    look.accent = c;
    for (int i = 0; i < kNumPages; ++i) tabs[i]->setToggleState (i == pg, juce::dontSendNotification);
    for (int i = 0; i < 8; ++i)
    {
        const int pi = pg * 8 + i;
        knobAtt[(size_t) i].reset();
        knobs[(size_t) i].setColour (juce::Slider::rotarySliderFillColourId, c);
        knobAtt[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, kParams[pi].id, knobs[(size_t) i]);
        knobNames[(size_t) i].setText (juce::String (kParams[pi].name).toUpperCase(), juce::dontSendNotification);
    }
    for (auto& m : macros) m.setColour (juce::Slider::trackColourId, c);
    screen.pageColour = pg;
    repaint();
}

void GrainChopEditor::refreshPads()
{
    const auto m = proc.music();
    const bool perPad = proc.param (pPerPad) > 0.5f;
    const int last = proc.lastPad.load();
    const int nsteps = (int) m.steps().size();
    for (int i = 0; i < 16; ++i)
    {
        auto* pd = pads[i];
        const int root = m.degMidi (i);
        const auto ns = m.chordFromDegree (i);
        pd->label = m.padLabel (i);
        const int sIdx = proc.padSrc[(size_t) i].load();
        pd->sub = perPad ? (sIdx >= kNumSources ? juce::String ("Random") : juce::String (kSourceNames[sIdx]))
                         : (m.chord > 0 ? Music::noteName (*std::min_element (ns.begin(), ns.end())) : "deg " + juce::String (i % nsteps + 1));
        pd->colour = hueFor (root);
        pd->isRoot = i % nsteps == 0;
        pd->current = perPad && i == last;
        pd->level = juce::jmin (1.0f, proc.engine.noteLevel[(size_t) juce::jlimit (0, 127, root)].load());
        pd->repaint();
    }
}

void GrainChopEditor::timerCallback()
{
    screen.pull();
    const int cp = proc.getCurrentProgram();
    if (cp != shownPreset) { shownPreset = cp; presetBox.setSelectedId (cp + 1, juce::dontSendNotification); }
    screen.presetLabel = juce::String (kPresets[cp].name).toUpperCase() + (proc.presetDirty.load() ? "*" : "");
    for (int i = 0; i < 8; ++i)
        if (auto* prm = proc.apvts.getParameter (kParams[page * 8 + i].id))
            knobValues[(size_t) i].setText (prm->getCurrentValueAsText(), juce::dontSendNotification);
    octLabel.setText ("Octave " + juce::String ((int) proc.param (pOct)), juce::dontSendNotification);
    refreshPads();
    screen.repaint();
}

void GrainChopEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::body);
    auto r = getLocalBounds().reduced (16, 12);
    auto head = r.removeFromTop (40);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.setColour (col::fg); g.drawText ("GRAIN", head.removeFromLeft (100), juce::Justification::centredLeft);
    g.setColour (look.accent); g.drawText ("CHOP", head.withTrimmedLeft (-4).removeFromLeft (90), juce::Justification::centredLeft);
    g.setColour (col::mute); g.setFont (juce::FontOptions (11.0f));
    g.drawText ("GRANULAR CHORD INSTRUMENT  /  PUSH LAYOUT", head.withTrimmedLeft (90).removeFromLeft (320), juce::Justification::centredLeft);

    // panels
    g.setColour (col::panel);
    auto deck = getLocalBounds().reduced (16, 12).withTrimmedTop (48).removeFromTop (330).toFloat();
    g.fillRoundedRectangle (deck, 10); g.setColour (col::edge); g.drawRoundedRectangle (deck.reduced (0.5f), 10, 1);
    auto lower = getLocalBounds().reduced (16, 12).withTrimmedTop (48 + 342).toFloat();
    auto padArea = lower.removeFromLeft (lower.getHeight() * 1.35f);
    g.setColour (col::panel); g.fillRoundedRectangle (padArea, 10); g.setColour (col::edge); g.drawRoundedRectangle (padArea.reduced (0.5f), 10, 1);
    lower.removeFromLeft (12);
    g.setColour (col::panel); g.fillRoundedRectangle (lower, 10); g.setColour (col::edge); g.drawRoundedRectangle (lower.reduced (0.5f), 10, 1);
}

void GrainChopEditor::resized()
{
    auto r = getLocalBounds().reduced (16, 12);
    auto head = r.removeFromTop (40);
    auto pr = head.removeFromRight (340).withSizeKeepingCentre (340, 30);
    prevPreset.setBounds (pr.removeFromLeft (34)); nextPreset.setBounds (pr.removeFromRight (34));
    presetBox.setBounds (pr.reduced (6, 0));
    r.removeFromTop (8);

    auto deck = r.removeFromTop (330).reduced (12);
    screen.setBounds (deck.removeFromTop (168));
    deck.removeFromTop (8);
    auto tabRow = deck.removeFromTop (26);
    const int tw = tabRow.getWidth() / kNumPages;
    for (int i = 0; i < kNumPages; ++i) tabs[i]->setBounds (tabRow.removeFromLeft (tw).reduced (2, 0));
    deck.removeFromTop (6);
    const int kw = deck.getWidth() / 8;
    for (int i = 0; i < 8; ++i)
    {
        auto c = deck.removeFromLeft (kw);
        knobNames[(size_t) i].setBounds (c.removeFromTop (16));
        knobValues[(size_t) i].setBounds (c.removeFromBottom (18));
        knobs[(size_t) i].setBounds (c.withSizeKeepingCentre (juce::jmin (c.getWidth(), c.getHeight()), juce::jmin (c.getWidth(), c.getHeight())));
    }

    r.removeFromTop (12);
    auto lower = r;
    auto padArea = lower.removeFromLeft ((int) (lower.getHeight() * 1.35f)).reduced (10);
    const int pw = padArea.getWidth() / 4, ph = padArea.getHeight() / 4;
    static const int order[16] = { 12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3 };
    for (int k = 0; k < 16; ++k) pads[order[k]]->setBounds (padArea.getX() + (k % 4) * pw, padArea.getY() + (k / 4) * ph, pw - 6, ph - 6);

    lower.removeFromLeft (12);
    auto side = lower.reduced (14, 10);
    auto colA = side.removeFromLeft (side.getWidth() / 2 - 8);
    side.removeFromLeft (16);
    auto colB = side;

    macroHdr.setBounds (colA.removeFromTop (16));
    for (int i = 0; i < kNumMacros; ++i)
    {
        auto row = colA.removeFromTop (30);
        macroNames[(size_t) i].setBounds (row.removeFromLeft (64));
        macros[(size_t) i].setBounds (row);
    }

    keyHdr.setBounds (colB.removeFromTop (16));
    auto kr = colB.removeFromTop (28); keyBox.setBounds (kr.removeFromLeft (70)); kr.removeFromLeft (6); scaleBox.setBounds (kr);
    colB.removeFromTop (6);
    auto orow = colB.removeFromTop (28); octDown.setBounds (orow.removeFromLeft (34)); octUp.setBounds (orow.removeFromRight (34)); octLabel.setBounds (orow);
    colB.removeFromTop (8);
    chordHdr.setBounds (colB.removeFromTop (16)); chordBox.setBounds (colB.removeFromTop (28));
    colB.removeFromTop (8);
    srcHdr.setBounds (colB.removeFromTop (16));
    auto srow = colB.removeFromTop (28); srcBox.setBounds (srow);
    colB.removeFromTop (6);
    auto prow = colB.removeFromTop (28); perPadBtn.setBounds (prow.removeFromLeft (prow.getWidth() / 2 - 3)); prow.removeFromLeft (6); randomBtn.setBounds (prow);
    colB.removeFromTop (8);
    perfHdr.setBounds (colB.removeFromTop (16));
    auto hrow = colB.removeFromTop (28); holdBtn.setBounds (hrow.removeFromLeft (hrow.getWidth() / 2 - 3)); hrow.removeFromLeft (6); stopBtn.setBounds (hrow);
}
