// Offline render test: tuning, stability and level across every preset.
#include "PluginProcessor.h"

using namespace gc;

static void render (GrainChopProcessor& p, juce::AudioBuffer<float>& out, double seconds, const std::vector<int>& notes, double sr)
{
    const int bs = 512, total = (int) (seconds * sr);
    out.setSize (2, total); out.clear();
    juce::AudioBuffer<float> b (2, bs);
    for (int pos = 0; pos < total; pos += bs)
    {
        juce::MidiBuffer m;
        if (pos == 0) for (int n : notes) m.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
        if (pos <= (int) (seconds * 0.7 * sr) && pos + bs > (int) (seconds * 0.7 * sr)) for (int n : notes) m.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        p.processBlock (b, m);
        const int nn = juce::jmin (bs, total - pos);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, b, c, 0, nn);
    }
}

static double pitchOf (const juce::AudioBuffer<float>& b, double sr, double t0, double t1)
{
    const int s = (int) (t0 * sr), e = (int) (t1 * sr), n = e - s;
    std::vector<double> x ((size_t) n);
    for (int i = 0; i < n; ++i) x[(size_t) i] = b.getSample (0, s + i) + b.getSample (1, s + i);
    // YIN difference function
    const int maxLag = (int) (sr / 70), minLag = (int) (sr / 1200), W = n - maxLag;
    std::vector<double> d ((size_t) maxLag + 1, 0.0), cm ((size_t) maxLag + 1, 1.0);
    for (int lag = 1; lag <= maxLag; ++lag) { double acc = 0; for (int i = 0; i < W; ++i) { const double q = x[(size_t) i] - x[(size_t) (i + lag)]; acc += q * q; } d[(size_t) lag] = acc; }
    double run = 0;
    for (int lag = 1; lag <= maxLag; ++lag) { run += d[(size_t) lag]; cm[(size_t) lag] = d[(size_t) lag] * lag / juce::jmax (1e-12, run); }
    int best = -1;
    for (int lag = minLag; lag < maxLag; ++lag) if (cm[(size_t) lag] < 0.15 && cm[(size_t) lag] <= cm[(size_t) lag + 1]) { best = lag; break; }
    if (best < 0) { double bv = 1e9; for (int lag = minLag; lag < maxLag; ++lag) if (cm[(size_t) lag] < bv) { bv = cm[(size_t) lag]; best = lag; } }
    const double a = cm[(size_t) best - 1], bb = cm[(size_t) best], c = cm[(size_t) best + 1];
    const double off = (a - 2 * bb + c) != 0 ? 0.5 * (a - c) / (a - 2 * bb + c) : 0;
    return sr / (best + off);
}

static void stats (const juce::AudioBuffer<float>& b, float& peak, float& rms, bool& finite)
{
    peak = 0; double acc = 0; finite = true;
    for (int c = 0; c < 2; ++c) for (int i = 0; i < b.getNumSamples(); ++i) { const float v = b.getSample (c, i); if (! std::isfinite (v)) finite = false; peak = juce::jmax (peak, std::abs (v)); acc += v * v; }
    rms = (float) std::sqrt (acc / (2.0 * b.getNumSamples()));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    int failures = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        GrainChopProcessor p;
        p.setPlayConfigDetails (0, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        // dry, clean setup for a tuning check: C major, octave 3, no FX, no detune/octave grains
        for (auto [i, v] : std::vector<std::pair<int, float>> { { pKey, 0 }, { pScale, 0 }, { pOct, 3 }, { pChord, 0 }, { pDmix, 0 }, { pGmix, 0 }, { pShimmer, 0 }, { pWow, 0 },
                                                                 { pFlutter, 0 }, { pHiss, 0 }, { pCrackle, 0 }, { pSat, 0 }, { pDarken, 0 }, { pDetune, 0 }, { pOctUp, 0 }, { pRev, 0 } })
            p.setParam (i, v);
        for (int src : { 3, 4, 14 })
            for (int note : { 48, 60, 67 })
            {
                p.setParam (pSrc, (float) src);
                p.prepareToPlay (sr, 512);
                juce::AudioBuffer<float> out; render (p, out, 1.6, { note }, sr);
                if (sr == 48000 && src == 3 && note == 60)
                {
                    juce::File fl ("/tmp/claude-0/-home-claude/16cdf013-b4c1-5bbf-9def-ca1f54a6b0af/scratchpad/tune.wav"); fl.deleteFile();
                    juce::WavAudioFormat wv;
                    if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wv.createWriterFor (new juce::FileOutputStream (fl), sr, 2, 24, {}, 0))) w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
                }
                if (gcBadStage) { std::printf ("NaN first at stage %d\n", gcBadStage); gcBadStage = 0; }
                const double f = pitchOf (out, sr, 0.3, 1.1), want = 440.0 * std::pow (2.0, (note - 69) / 12.0);
                double cents = 1200 * std::log2 (f / want);
                // the YIN estimate may lock to an octave of the fundamental; fold that in and report it
                const double folded = cents - 1200 * std::round (cents / 1200);
                const bool ok = std::abs (folded) < 12;
                if (! ok) ++failures;
                std::printf ("sr %6.0f  %-12s note %d  want %7.2f Hz  got %7.2f Hz  %+6.1f ct %s\n", sr, kSourceNames[src], note, want, f, folded, ok ? "ok" : "OUT OF TUNE");
            }
    }

    // every preset: hold a chord, release, let the tail ring
    GrainChopProcessor p;
    const double sr = 48000;
    p.setPlayConfigDetails (0, 2, sr, 512);
    p.prepareToPlay (sr, 512);
    for (int i = 0; i < kNumPresets; ++i)
    {
        p.setCurrentProgram (i);
        p.prepareToPlay (sr, 512);
        juce::AudioBuffer<float> out; render (p, out, 3.0, { 62, 65, 69 }, sr);
        float pk, rms; bool fin; stats (out, pk, rms, fin);
        const bool ok = fin && pk <= 1.0f && rms > 0.005f;
        if (! ok) ++failures;
        std::printf ("%2d %-13s %-20s peak %.3f  rms %.3f %s\n", i + 1, kPresets[i].category, kPresets[i].name, pk, rms, ok ? "ok" : "FAIL");
        if (i == 0)
        {
            juce::File f ("/tmp/claude-0/-home-claude/16cdf013-b4c1-5bbf-9def-ca1f54a6b0af/scratchpad/init_chord.wav"); f.deleteFile();
            juce::WavAudioFormat wav;
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 16, {}, 0))) w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
        }
    }
    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL OK" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
