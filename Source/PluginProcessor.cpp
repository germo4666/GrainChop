#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

using namespace gc;

juce::AudioProcessorValueTreeState::ParameterLayout GrainChopProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& d = kParams[i];
        const juce::ParameterID pid { d.id, 1 };
        if (d.fmt == Fmt::Choice)
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, d.name, juce::StringArray::fromTokens (d.choices, "|", ""), (int) d.def));
        else if (d.fmt == Fmt::Bool)
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > 0.5f));
        else
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, d.name, makeRange (d), d.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([&d] (float v, int) { return formatValue (d, v); })));
    }
    return layout;
}

GrainChopProcessor::GrainChopProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GrainChop", createLayout())
{
    for (int i = 0; i < kNumParams; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue (kParams[i].id);
        apvts.addParameterListener (kParams[i].id, this);
    }
    for (int i = 0; i < 16; ++i) padSrc[(size_t) i] = i;

    // the 16 tuned sources, rendered from the same synthesis as the browser demo
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (new juce::MemoryInputStream (BinaryData::sources_wav, BinaryData::sources_wavSize, false), true));
    if (reader != nullptr)
    {
        engine.src.setSize (2, (int) reader->lengthInSamples);
        reader->read (&engine.src, 0, (int) reader->lengthInSamples, 0, true, true);
        engine.srcRate = reader->sampleRate;
    }
    else engine.src.setSize (2, 1024);
    startTimerHz (20);
}

GrainChopProcessor::~GrainChopProcessor()
{
    stopTimer();
    for (int i = 0; i < kNumParams; ++i) apvts.removeParameterListener (kParams[i].id, this);
}

bool GrainChopProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void GrainChopProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    engine.makeIR (param (pDecay));
    uiMidi.reset (sampleRate);
}

Music GrainChopProcessor::music() const
{
    Music m; m.key = (int) param (pKey); m.scale = (int) param (pScale); m.oct = (int) param (pOct); m.chord = (int) param (pChord);
    return m;
}

void GrainChopProcessor::handleNote (int trigger, int note, float velocity, bool fromPads)
{
    const Music m = music();
    const bool padMode = fromPads || param (pInput) > 0.5f;
    int degree;
    if (padMode && note >= 36 && note < 52) degree = note - 36;
    else degree = m.degreeOf (note);
    const int pad = ((degree % 16) + 16) % 16;
    lastPad.store (pad);

    if (param (pHold) > 0.5f && engine.triggerSounding (trigger)) { engine.noteOff (trigger); return; }

    const float velo = param (pVelo);
    const float vel = 0.9f * ((1.0f - velo) + velo * velocity);
    const int srcIdx = param (pPerPad) > 0.5f ? padSrc[(size_t) pad].load() : -1;
    engine.noteOn (trigger, m.chordFromDegree (degree), vel, srcIdx, pad, p.data());
}

void GrainChopProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    for (int i = 0; i < kNumParams; ++i) p[(size_t) i] = raw[(size_t) i]->load();

    const int n = buffer.getNumSamples();
    buffer.clear();

    // tempo + beat position: follow the host when it is playing, free-run otherwise
    double bpm = p[pBpm];
    bool hostPlaying = false; double hostPpq = 0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (p[pSync] > 0.5f) if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying()) if (auto q = pos->getPpqPosition()) { hostPlaying = true; hostPpq = *q; }
        }
    bpm = juce::jlimit (20.0, 400.0, bpm);
    if (hostPlaying) internalPpq = hostPpq;

    if (stopRequest.exchange (false)) engine.releaseAll();

    juce::MidiBuffer padMidi;
    uiMidi.removeNextBlockOfMessages (padMidi, n);

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (buffer.getNumChannels() > 1 ? 1 : 0);
    int cursor = 0;
    const double ppqPerSample = bpm / 60.0 / sr;

    auto renderTo = [&] (int end)
    {
        if (end > cursor) { engine.render (L + cursor, R + cursor, end - cursor, p.data(), internalPpq + cursor * ppqPerSample, bpm, modwheel); cursor = end; }
    };
    auto handle = [&] (const juce::MidiMessage& m, bool fromPads)
    {
        const int trig = m.getChannel() * 128 + m.getNoteNumber() + (fromPads ? 4096 : 0);
        if (m.isNoteOn()) handleNote (trig, m.getNoteNumber(), m.getFloatVelocity(), fromPads);
        else if (m.isNoteOff()) { if (p[pHold] < 0.5f) engine.noteOff (trig); }
        else if (m.isController() && m.getControllerNumber() == 1) modwheel = m.getControllerValue() / 127.0f;
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.releaseAll();
    };

    for (const auto meta : padMidi) handle (meta.getMessage(), true);
    for (const auto meta : midi)
    {
        renderTo (juce::jlimit (0, n, meta.samplePosition));
        handle (meta.getMessage(), false);
    }
    renderTo (n);
    internalPpq += n * ppqPerSample;
    midi.clear();
}

// ---------------------------------------------------------------- params & presets
void GrainChopProcessor::setParam (int i, float v)
{
    if (auto* prm = apvts.getParameter (kParams[i].id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

void GrainChopProcessor::parameterChanged (const juce::String& id, float)
{
    if (applyingPreset.load()) return;
    for (int i = 0; i < kNumParams; ++i)
        if (id == kParams[i].id) { if (! presetKeeps (i)) presetDirty.store (true); return; }
}

void GrainChopProcessor::setCurrentProgram (int index)
{
    index = juce::jlimit (0, kNumPresets - 1, index);
    applyingPreset.store (true);
    std::array<float, kNumParams> vals {};
    for (int i = 0; i < kNumParams; ++i) vals[(size_t) i] = kParams[i].def;
    for (auto& tok : juce::StringArray::fromTokens (kPresets[index].values, ",", ""))
    {
        const auto k = tok.upToFirstOccurrenceOf (":", false, false).trim();
        const auto v = tok.fromFirstOccurrenceOf (":", false, false).trim().getFloatValue();
        for (int i = 0; i < kNumParams; ++i) if (k == kParams[i].id) vals[(size_t) i] = v;
    }
    for (int i = 0; i < kNumParams; ++i) if (! presetKeeps (i)) setParam (i, vals[(size_t) i]);
    applyingPreset.store (false);
    currentPreset.store (index);
    presetDirty.store (false);
}

const juce::String GrainChopProcessor::getProgramName (int index)
{
    index = juce::jlimit (0, kNumPresets - 1, index);
    return juce::String (kPresets[index].category) + ": " + kPresets[index].name;
}

void GrainChopProcessor::randomizePadSources()
{
    juce::Random r;
    for (auto& s : padSrc) s.store (r.nextInt (kNumSources + 1));
    setParam (pPerPad, 1.0f);
    lastSyncedPad = -1;
}

void GrainChopProcessor::timerCallback()
{
    // Glass reverb IR: rebuilt on the message thread when the size changes
    if (std::abs (param (pDecay) - engine.currentDecay()) > 0.05f) engine.makeIR (param (pDecay));

    // per-pad sources: the Source knob shows / edits the last pad you played
    if (param (pPerPad) > 0.5f)
    {
        const int pad = lastPad.load();
        const int s = (int) param (pSrc);
        if (pad != lastSyncedPad)
        {
            lastSyncedPad = pad;
            const bool was = applyingPreset.exchange (true);
            setParam (pSrc, (float) padSrc[(size_t) pad].load());
            applyingPreset.store (was);
            lastSeenSrc = padSrc[(size_t) pad].load();
        }
        else if (s != lastSeenSrc) { padSrc[(size_t) pad].store (s); lastSeenSrc = s; }
    }
    else { lastSyncedPad = -1; lastSeenSrc = (int) param (pSrc); }
}

void GrainChopProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::StringArray ps; for (auto& s : padSrc) ps.add (juce::String (s.load()));
    state.setProperty ("padSrc", ps.joinIntoString (","), nullptr);
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void GrainChopProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            auto ps = juce::StringArray::fromTokens (tree.getProperty ("padSrc").toString(), ",", "");
            for (int i = 0; i < 16 && i < ps.size(); ++i) padSrc[(size_t) i].store (juce::jlimit (0, kSrcRandom, ps[i].getIntValue()));
            currentPreset.store (juce::jlimit (0, kNumPresets - 1, (int) tree.getProperty ("preset", 0)));
            applyingPreset.store (true);
            apvts.replaceState (tree);
            applyingPreset.store (false);
        }
}

juce::AudioProcessorEditor* GrainChopProcessor::createEditor() { return new GrainChopEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GrainChopProcessor(); }
