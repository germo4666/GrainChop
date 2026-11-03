#pragma once
#include "Defs.h"
#include <atomic>
#include <random>

// ============================================================================
//  GrainChop DSP: granular note engine + Dirt / Glass / Tape / Pump / Gate /
//  Massive FX chain. Everything runs per sample on the audio thread with no
//  allocation after prepare().
// ============================================================================

#ifdef GC_DEBUG
inline int gcBadStage = 0;
#define GC_CHECK(v, id) if (! std::isfinite (v) && gcBadStage == 0) gcBadStage = id;
#else
#define GC_CHECK(v, id)
#endif
namespace gc
{
struct Rng
{
    juce::uint32 s = 0x9e3779b9u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 16777216.0f; } // 0..1
    float bi() { return next() * 2.0f - 1.0f; }
};

// RBJ biquad, coefficients match Web Audio's BiquadFilterNode so the plugin sounds like the demo.
struct Biquad
{
    enum Type { LP, HP, BP, LowShelf, HighShelf, Peak };
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1[2] {}, z2[2] {};
    void set (Type t, double fs, double f, double q, double gainDb = 0)
    {
        f = juce::jlimit (10.0, fs * 0.49, f);
        const double w = juce::MathConstants<double>::twoPi * f / fs, cw = std::cos (w), sw = std::sin (w);
        const double A = std::pow (10.0, gainDb / 40.0);
        double B0, B1, B2, A0, A1, A2;
        if (t == LP || t == HP)
        {
            const double Q = std::pow (10.0, q / 20.0), al = sw / (2 * Q);
            if (t == LP) { B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = B0; }
            else         { B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = B0; }
            A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al;
        }
        else if (t == BP)
        {
            const double al = sw / (2 * juce::jmax (0.0001, q));
            B0 = al; B1 = 0; B2 = -al; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al;
        }
        else if (t == Peak)
        {
            const double al = sw / (2 * juce::jmax (0.0001, q));
            B0 = 1 + al * A; B1 = -2 * cw; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * cw; A2 = 1 - al / A;
        }
        else
        {
            const double al = sw / 2 * std::sqrt (2.0), sq = 2 * std::sqrt (A) * al;
            if (t == LowShelf)
            {
                B0 = A * ((A + 1) - (A - 1) * cw + sq); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - sq);
                A0 = (A + 1) + (A - 1) * cw + sq; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - sq;
            }
            else
            {
                B0 = A * ((A + 1) + (A - 1) * cw + sq); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - sq);
                A0 = (A + 1) - (A - 1) * cw + sq; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - sq;
            }
        }
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }
    inline float process (float x, int ch)
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
};

struct Smooth
{
    float v = 0, t = 0, k = 0.001f;
    void setTime (double fs, double sec) { k = (float) (1.0 - std::exp (-1.0 / (sec * fs))); }
    inline float next() { v += (t - v) * k; return v; }
    void snap (float x) { v = t = x; }
};

struct Grain
{
    double pos = 0, inc = 0; // source read position & step (samples of the source)
    int len = 0, age = 0;
    float amp = 0, gl = 0, gr = 0;
    int voice = -1; juce::uint32 gen = 0;
    bool active = false, toGlass = false, soft = false;
};

struct Voice
{
    bool active = false, released = false;
    int midi = 60, src = -1, padIndex = 0, trigger = -1; // trigger: incoming note number that started it
    juce::uint32 gen = 0;
    float vel = 1, env = 0, stepAmp = 1, pos = 0.4f;
    double countdown = 0;
    juce::int64 lastK = -1;
};

struct GrainEvent { int midi; float p; float absSec; float sizeSec; float width; bool rev; };

class Engine
{
public:
    static constexpr int kMaxGrains = 768, kMaxVoices = 48;

    juce::AudioBuffer<float> src; // stereo, 44.1 kHz
    double srcRate = 44100.0;

    // UI feedback
    juce::AbstractFifo eventFifo { 1024 };
    std::array<GrainEvent, 1024> events {};
    std::array<std::atomic<float>, 128> noteLevel {};
    std::atomic<int> grainsPerSec { 0 };

    void prepare (double sampleRate, int /*blockSize*/)
    {
        fs = sampleRate;
        tapeDelay.assign ((size_t) (fs * 0.1) + 4, 0.0f); tapeDelayR.assign (tapeDelay.size(), 0.0f); tapeW = 0;
        for (auto& l : mLines) { l.buf.assign ((size_t) (fs * 4.0) + 4, 0.0f); l.w = 0; l.lp.reset(); l.lfoPh = rng.next(); }
        for (auto& g : grains) g.active = false;
        for (auto& v : voices) v.active = false;
        glass.reset();
        juce::dsp::ProcessSpec spec { fs, 2048, 2 };
        glass.prepare (spec);
        glassBuf.setSize (2, 2048);
        lastDecay = -1; firstBlock = true;
        for (auto* b : { &tone, &mass, &glassHP, &tLow, &tDark, &tShelf, &hissBP, &crkHP, &mLow }) b->reset();
        for (auto& f : form) f.reset();
        pumpG = 1; gateG = 1; limEnv = 0;
        for (auto* s : { &sDmix, &sGmix, &sMmix, &sMaster, &sMsize })
            s->setTime (fs, 0.02);
        sMsize.setTime (fs, 0.08);
        softTable.resize (256); envTable.resize (256);
        buildEnv (softTable, 0.0f); lastShape = -1;
        samplesSinceGps = 0; gpsCount = 0;
    }

    // ---------------------------------------------------------------- notes
    void noteOn (int trigger, const std::vector<int>& midis, float vel, int srcIdx, int padIndex, const float* p)
    {
        for (size_t k = 0; k < midis.size(); ++k)
        {
            int slot = -1;
            for (int i = 0; i < kMaxVoices; ++i) if (! voices[(size_t) i].active) { slot = i; break; }
            if (slot < 0) // steal the quietest released voice
            {
                float best = 2; for (int i = 0; i < kMaxVoices; ++i) if (voices[(size_t) i].released && voices[(size_t) i].env < best) { best = voices[(size_t) i].env; slot = i; }
                if (slot < 0) slot = 0;
            }
            auto& v = voices[(size_t) slot];
            v.active = true; v.released = false; v.midi = midis[k]; v.src = srcIdx; v.padIndex = padIndex; v.trigger = trigger;
            v.gen = ++genCounter; v.vel = vel * (k ? 0.8f : 1.0f); v.env = 0; v.stepAmp = 1; v.pos = p[pPos]; v.countdown = fs * 0.004; v.lastK = -1;
        }
    }
    void noteOff (int trigger)
    {
        for (auto& v : voices) if (v.active && ! v.released && v.trigger == trigger) v.released = true;
    }
    void releaseAll() { for (auto& v : voices) if (v.active) v.released = true; }
    bool triggerSounding (int trigger) const { for (auto& v : voices) if (v.active && ! v.released && v.trigger == trigger) return true; return false; }

    // ---------------------------------------------------------------- render
    // p: current parameter values (plain), ppq0: beat position at block start, bpm: tempo
    void render (float* L, float* R, int n, const float* p, double ppq0, double bpm, float modwheel)
    {
        const double ppqPerSample = bpm / 60.0 / fs;
        updateBlock (p, bpm);
        const float relCoef = (float) (1.0 - std::exp (-1.0 / (juce::jmax (0.001f, p[pRelease] / 4.0f) * fs)));
        const float attCoef = (float) (1.0 - std::exp (-1.0 / (juce::jmax (0.002f, p[pAttack] / 3.0f) * fs)));
        const double effSize = juce::jlimit (8.0, 700.0, (double) p[pSize] * std::pow (28.0 / p[pGrains], 0.6 * p[pLink])) / 1000.0;
        const int pat = juce::jlimit (0, 5, (int) p[pGpat]);
        const double rateBeats = kRateBeats[juce::jlimit (0, 6, (int) p[pRate])];
        const float srcRatio = (float) (srcRate / fs);
        const int srcLen = src.getNumSamples();
        const float* sL = src.getReadPointer (0);
        const float* sR = src.getReadPointer (src.getNumChannels() > 1 ? 1 : 0);

        for (int i = 0; i < n; ++i)
        {
            const double ppq = ppq0 + i * ppqPerSample;
            // ---- voices: envelopes + grain scheduling
            for (int vi = 0; vi < kMaxVoices; ++vi)
            {
                auto& v = voices[(size_t) vi];
                if (! v.active) continue;
                if (v.released) { v.env += (0.0f - v.env) * relCoef; if (v.env < 0.0002f) { v.active = false; continue; } }
                else v.env += (1.0f - v.env) * attCoef;
                v.countdown -= 1.0;
                if (v.countdown > 0) continue;
                const double dtSec = (1.0 / p[pGrains]) * juce::jlimit (0.25, 2.0, 1.0 + rng.bi() * p[pRnd] * 0.7);
                v.countdown += dtSec * fs;
                bool on = true;
                if (rateBeats > 0)
                {
                    const double ph = ppq / rateBeats; const auto k = (juce::int64) std::floor (ph);
                    double w = ph - (double) k;
                    if (k % 2 != 0) w -= (p[pSwing] - 50.0) / 50.0;
                    on = w >= 0 && w < p[pGate] / 100.0;
                    if (on && k != v.lastK) { v.lastK = k; v.stepAmp = 1.0f - p[pHuman] * rng.next(); }
                }
                const float target = juce::jlimit (0.0f, 1.0f, p[pPos] + modwheel);
                v.pos = juce::jlimit (0.0f, 1.0f, v.pos + (float) ((rng.bi() * p[pDrift] * 1.2f + (target - v.pos) * 0.8f) * dtSec));
                if (on) spawn (v, vi, p, effSize, ppq, srcLen);
            }

            // ---- grains
            float mL = 0, mR = 0, gL = 0, gR = 0;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                const auto& v = voices[(size_t) g.voice];
                if (v.gen != g.gen || g.age >= g.len) { g.active = false; continue; }
                const int i0 = (int) g.pos; const float fr = (float) (g.pos - i0);
                if (i0 < 0 || i0 + 1 >= srcLen) { g.active = false; continue; }
                const float xl = sL[i0] + (sL[i0 + 1] - sL[i0]) * fr;
                const float xr = sR[i0] + (sR[i0 + 1] - sR[i0]) * fr;
                const float x = (float) g.age / (float) g.len * 255.0f; const int ti = (int) x; const float tf = x - (float) ti;
                const auto& tab = g.soft ? softTable : envTable;
                const float e = (tab[(size_t) ti] + (tab[(size_t) juce::jmin (255, ti + 1)] - tab[(size_t) ti]) * tf) * g.amp * v.env;
                if (g.toGlass) { gL += xl * e; gR += xr * e; }
                else { mL += xl * e * g.gl; mR += xr * e * g.gr; }
                g.pos += g.inc * srcRatio; ++g.age;
            }

            float oL, oR;
            fxChain (mL, mR, gL, gR, ppq, bpm, pat, p, oL, oR);
            L[i] = oL; R[i] = oR;
        }

        // UI: note levels + grains/s
        for (auto& a : noteLevel) a.store (0.0f, std::memory_order_relaxed);
        for (auto& v : voices) if (v.active) { auto& a = noteLevel[(size_t) juce::jlimit (0, 127, v.midi)]; a.store (juce::jmax (a.load (std::memory_order_relaxed), v.released ? v.env * 0.6f : v.env), std::memory_order_relaxed); }
        samplesSinceGps += n;
        if (samplesSinceGps >= fs) { grainsPerSec.store (gpsCount); gpsCount = 0; samplesSinceGps = 0; }
    }

    int activeGrains() const { int c = 0; for (auto& g : grains) c += g.active; return c; }
    int activeVoices() const { int c = 0; for (auto& v : voices) c += v.active; return c; }
    float voiceEnv() const { for (auto& v : voices) if (v.active) return v.env; return -1; }
private:
    double fs = 44100;
    Rng rng;
    std::array<Grain, kMaxGrains> grains {};
    std::array<Voice, kMaxVoices> voices {};
    juce::uint32 genCounter = 0;
    std::vector<float> softTable, envTable;
    float lastShape = -1;
    int samplesSinceGps = 0, gpsCount = 0;

    static void buildEnv (std::vector<float>& t, float s)
    {
        for (int i = 0; i < 256; ++i)
        {
            const float x = (float) i / 255.0f;
            const float soft = std::pow (std::sin (juce::MathConstants<float>::pi * x), 2.0f);
            const float perc = juce::jmin (1.0f, x / 0.04f) * std::exp (-x * 4.0f) * (1.0f - x);
            t[(size_t) i] = soft * (1 - s) + perc * s * 1.4f;
        }
        t[255] = 0;
    }

    void spawn (Voice& v, int vi, const float* p, double eff, double ppq, int srcLen)
    {
        int si = v.src >= 0 ? v.src : juce::jlimit (0, kSrcRandom, (int) p[pSrc]);
        if (si >= kNumSources) si = (int) (rng.next() * kNumSources) % kNumSources;
        const double s0 = sliceStart (si), dur = sliceEnd (si) - s0;
        const double size = eff * juce::jlimit (0.3, 2.0, 1.0 + rng.bi() * p[pRnd] * 0.6);
        const float pp = juce::jlimit (0.0f, 1.0f, v.pos + rng.bi() * p[pSpray] * 0.5f);
        double semis = v.midi - sourceRoot (si) + p[pTune] + rng.bi() * p[pDetune] / 100.0;
        const float r = rng.next();
        if (r < p[pOctUp] * 0.5f) semis += 12; else if (r < (p[pOctUp] + p[pOctDn]) * 0.5f) semis -= 12;
        double rate = std::pow (2.0, semis / 12.0);
        if (p[pWobble] > 0) { const double ph = ppq - std::floor (ppq); rate *= 1.0 - p[pWobble] * 0.07 * std::exp (-ph * 5.0); }
        const bool rev = rng.next() < p[pRev];
        const double span = size * rate, at = s0 + pp * dur;
        const float amp = v.vel * v.stepAmp * 0.6f * juce::jmin (1.0f, (float) (1.3 / std::sqrt (juce::jmax (1.0, p[pGrains] * eff))));
        const int len = juce::jmax (8, (int) (size * fs));
        const double startSec = rev ? at + span : at;
        const double maxPos = srcLen - 2.0;

        auto launch = [&] (double rt, bool toGlass, float a, bool soft)
        {
            for (auto& g : grains)
            {
                if (g.active) continue;
                g.active = true; g.toGlass = toGlass; g.soft = soft; g.voice = vi; g.gen = v.gen;
                g.pos = juce::jlimit (0.0, maxPos, startSec * srcRate); g.inc = rev ? -rt : rt;
                g.len = len; g.age = 0; g.amp = a;
                const float pan = rng.bi() * p[pWidth];
                g.gl = std::cos ((pan + 1) * juce::MathConstants<float>::pi / 4) * 1.4142f;
                g.gr = std::sin ((pan + 1) * juce::MathConstants<float>::pi / 4) * 1.4142f;
                return true;
            }
            return false;
        };
        if (! launch (rate, false, amp, false)) return;
        ++gpsCount;
        if (p[pGmix] > 0.02f && p[pShimmer] > 0.02f && rng.next() < p[pShimmer] * 0.6f) launch (rate * 2.0, true, amp * 0.5f, true);

        int start1, size1, start2, size2;
        eventFifo.prepareToWrite (1, start1, size1, start2, size2);
        if (size1 > 0)
            events[(size_t) start1] = { v.midi, pp, (float) at, (float) size, (float) juce::jmin (0.5, span / dur), rev };
        eventFifo.finishedWrite (size1);
    }

    // ---------------------------------------------------------------- FX state
    bool firstBlock = true;
    Biquad form[3], tone, mass, glassHP, tLow, tDark, tShelf, hissBP, crkHP, mLow;
    float crushLevels = 0, preGain = 1, compGain = 1, vmix = 0, dlevel = 1, tPre = 1, tComp = 1, hissG = 0, crkG = 0;
    float fuzzK = 1.2f, fuzzN = 1, tapeK = 1.4f, tapeN = 1;
    juce::dsp::Convolution glass { juce::dsp::Convolution::NonUniform { 512 } };
    juce::AudioBuffer<float> glassBuf;
    std::vector<float> glassIn[2];
    int glassFill = 0; float lastDecay = -1;
    std::vector<float> tapeDelay, tapeDelayR; int tapeW = 0;
    double wowPh = 0, driftPh = 0, flPh = 0, fl2Ph = 0;
    float wowAmp = 0, driftAmp = 0, flAmp = 0, fl2Amp = 0, wowRate = 0.55f;
    float pumpG = 1, pumpHold = 0, pumpK = 0, gateG = 1, gateK = 0;
    double lastBeat = -1, lastStep = -1;
    float crkEnv[2] {}, crkAmp[2] {};
    float limEnv = 0, limAtt = 0, limRel = 0;
    Smooth sDmix, sGmix, sMmix, sMaster, sMsize;
    float glassOutL = 0, glassOutR = 0;

    struct Line { std::vector<float> buf; int w = 0; Biquad lp; double lfoPh = 0; };
    std::array<Line, 8> mLines;
    float mFb = 0.8f, mHouse = -0.2f, mWarp = 0.4f, mRate = 1;
    static constexpr float kMBase[8] = { .0437f, .0523f, .0613f, .0719f, .0811f, .0937f, .1031f, .1171f };
    static constexpr float kMLfo[8]  = { .11f, .13f, .17f, .19f, .23f, .29f, .31f, .37f };

public:
    // call from the message thread (or prepare); the convolution swaps IRs safely in the background
    void makeIR (float decay)
    {
        lastDecay = decay;
        const int len = (int) (fs * decay);
        juce::AudioBuffer<float> ir (2, len);
        Rng r; r.s = 12345;
        for (int c = 0; c < 2; ++c)
        {
            auto* x = ir.getWritePointer (c);
            for (int i = 0; i < len; ++i)
            {
                const double t = i / fs;
                x[i] = (float) (r.bi() * (1 - std::exp (-t / 0.07)) * std::exp (-t * 3 / decay) * (1.0 - (double) i / len));
            }
        }
        glass.loadImpulseResponse (std::move (ir), fs, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
    }
    float currentDecay() const { return lastDecay; }
private:

    void updateBlock (const float* p, double bpm)
    {
        if (firstBlock) { firstBlock = false; sDmix.snap (p[pDmix]); sGmix.snap (p[pGmix]); sMmix.snap (p[pMmix]); sMaster.snap (p[pMaster]); sMsize.snap (p[pMsize]); }
        if (std::abs (p[pShape] - lastShape) > 1e-4f) { lastShape = p[pShape]; buildEnv (envTable, lastShape); }
        sDmix.t = p[pDmix]; sGmix.t = p[pGmix]; sMmix.t = p[pMmix]; sMaster.t = p[pMaster]; sMsize.t = p[pMsize];
        const int bits = (int) p[pBits];
        crushLevels = bits >= 12 ? 0.0f : std::pow (2.0f, (float) bits - 1);
        const float vw = p[pVowel]; const int vi = (int) std::floor (vw); const float vf = vw - (float) vi; const int vj = juce::jmin (4, vi + 1);
        static const float qs[3] = { 8, 11, 14 };
        for (int k = 0; k < 3; ++k) form[k].set (Biquad::BP, fs, kVowels[vi][k] + (kVowels[vj][k] - kVowels[vi][k]) * vf, qs[k]);
        vmix = p[pVmix]; preGain = 1 + p[pDrive] * 28; compGain = 1 / (1 + p[pDrive] * 2.2f); dlevel = p[pDlevel];
        fuzzN = 1.0f / std::tanh (fuzzK);
        tone.set (Biquad::LP, fs, p[pTone], 0.9);
        mass.set (Biquad::LowShelf, fs, 90, 0, p[pMass]);
        glassHP.set (Biquad::HP, fs, 260, 0.7);
        // tape
        tPre = 1 + p[pSat] * 5; tComp = 1 / (1 + p[pSat] * 2.2f); tapeN = 1.0f / std::tanh (tapeK);
        wowRate = p[pWrate];
        wowAmp = p[pWow] * 0.009f / (juce::MathConstants<float>::twoPi * wowRate); driftAmp = wowAmp * 0.6f;
        flAmp = p[pFlutter] * 0.0024f / (juce::MathConstants<float>::twoPi * 9.3f); fl2Amp = p[pFlutter] * 0.0012f / (juce::MathConstants<float>::twoPi * 13.7f);
        tLow.set (Biquad::HP, fs, p[pLowcut], 0.6);
        tDark.set (Biquad::LP, fs, 20000.0 * std::pow (1000.0 / 20000.0, (double) p[pDarken]), 0.5);
        tShelf.set (Biquad::HighShelf, fs, 3500, 0, -p[pDarken] * 10.0);
        hissBP.set (Biquad::BP, fs, 5200, 0.4); crkHP.set (Biquad::HP, fs, 1200, 0.5);
        hissG = p[pHiss] * p[pHiss] * 0.05f; crkG = p[pCrackle] * 0.6f;
        pumpK = (float) (1.0 - std::exp (-1.0 / (60.0 / bpm * 0.16 * fs)));
        gateK = (float) (1.0 - std::exp (-1.0 / (0.002 * fs)));
        // massive
        mFb = p[pMfb] * 0.985f; mHouse = -0.25f * p[pMdens]; mWarp = p[pMwarp]; mRate = p[pMrate];
        const double damp = 18000.0 * std::pow (600.0 / 18000.0, (double) p[pMdamp]);
        for (auto& l : mLines) l.lp.set (Biquad::LP, fs, damp, 0.5);
        mLow.set (Biquad::HP, fs, p[pMlow], 0.6);
        limAtt = (float) (1.0 - std::exp (-1.0 / (0.002 * fs))); limRel = (float) (1.0 - std::exp (-1.0 / (0.12 * fs)));
    }

    inline float readDelay (const std::vector<float>& b, int w, double delaySamples) const
    {
        const int N = (int) b.size();
        double rp = w - delaySamples; while (rp < 0) rp += N;
        const int i0 = (int) rp; const float f = (float) (rp - i0); const int i1 = (i0 + 1) % N;
        return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * f;
    }

    void fxChain (float inL, float inR, float shL, float shR, double ppq, double bpm, int pat, const float* p, float& outL, float& outR)
    {
        const float dmix = sDmix.next(), gmix = sGmix.next(), mmix = sMmix.next(), master = sMaster.next(), msize = sMsize.next();
        float x[2] = { inL, inR }, post[2];
        // glass input is buffered in blocks for the convolution engine (adds a few ms of pre-delay, which suits a reverb)
        for (int c = 0; c < 2; ++c)
        {
            const float s = x[c];
            const float clean = s * (1 - dmix * 0.85f);
            // DIRT
            float d = s * dmix;
            if (crushLevels > 0) d = std::round (juce::jlimit (-1.0f, 1.0f, d) * crushLevels) / crushLevels;
            float fw = 0; static const float fg[3] = { 1.0f * 3.2f, 0.6f * 3.2f, 0.35f * 3.2f };
            for (int k = 0; k < 3; ++k) fw += form[k].process (d, c) * fg[k];
            float pre = (d * (1 - vmix) + fw * vmix) * preGain;
            pre = std::tanh (fuzzK * pre) * fuzzN;
            pre = mass.process (tone.process (pre, c), c) * compGain * dlevel;
            post[c] = clean + pre;
            GC_CHECK (post[c], 1);
        }
        // GLASS (convolution, processed in 256-sample chunks)
        const float gin[2] = { x[0] * gmix + shL, x[1] * gmix + shR };
        if (glassIn[0].size() != 256) { glassIn[0].assign (256, 0); glassIn[1].assign (256, 0); glassFill = 0; }
        glassIn[0][(size_t) glassFill] = gin[0]; glassIn[1][(size_t) glassFill] = gin[1];
        const float gOutL = glassBuf.getSample (0, glassFill), gOutR = glassBuf.getSample (1, glassFill);
        if (++glassFill >= 256)
        {
            glassFill = 0;
            glassBuf.setSize (2, 256, false, false, true);
            for (int c = 0; c < 2; ++c) glassBuf.copyFrom (c, 0, glassIn[c].data(), 256);
            juce::dsp::AudioBlock<float> blk (glassBuf);
            glass.process (juce::dsp::ProcessContextReplacing<float> (blk));
            for (int c = 0; c < 2; ++c) { auto* w = glassBuf.getWritePointer (c); for (int i = 0; i < 256; ++i) w[i] = glassHP.process (w[i], c) * 0.8f; }
        }
        post[0] += gOutL; post[1] += gOutR;
        GC_CHECK (gOutL, 2);

        // TAPE
        const double dt = 1.0 / fs;
        wowPh += wowRate * dt; driftPh += 0.13 * dt; flPh += 9.3 * dt; fl2Ph += 13.7 * dt;
        const double tw = juce::MathConstants<double>::twoPi;
        const double delaySec = 0.022 + wowAmp * std::sin (tw * wowPh) + driftAmp * std::sin (tw * driftPh) + flAmp * std::sin (tw * flPh) + fl2Amp * std::sin (tw * fl2Ph);
        float tape[2];
        for (int c = 0; c < 2; ++c)
        {
            float s = std::tanh (tapeK * post[c] * tPre) * tapeN * tComp;
            s += hissBP.process (rng.bi(), c) * hissG;
            if (crkG > 0 && rng.next() < 15.0f / (float) fs) { const float a = std::pow (rng.next(), 3.0f); crkAmp[c] = rng.next() < 0.5f ? -a : a; crkEnv[c] = 1; }
            crkEnv[c] *= (float) std::exp (-1.0 / 6.0 * 44100.0 / fs);
            s += crkHP.process (crkAmp[c] * crkEnv[c], c) * crkG;
            auto& buf = c == 0 ? tapeDelay : tapeDelayR;
            buf[(size_t) tapeW] = s;
            float y = readDelay (buf, tapeW, delaySec * fs);
            y = tShelf.process (tDark.process (tLow.process (y, c), c), c);
            GC_CHECK (s, 3); GC_CHECK (y, 4);
            tape[c] = y;
        }
        tapeW = (tapeW + 1) % (int) tapeDelay.size();

        // PUMP + SILENCE GATE (tempo synced)
        if (p[pPump] > 0)
        {
            const double beat = std::floor (ppq);
            if (beat != lastBeat) { lastBeat = beat; pumpG = 1 - p[pPump] * 0.9f; pumpHold = (float) (0.005 * fs); }
            if (pumpHold > 0) pumpHold -= 1; else pumpG += (1 - pumpG) * pumpK;
        }
        else pumpG = 1;
        const double step16 = std::floor (ppq * 4.0);
        const int idx = (int) ((juce::int64) ((p[pGrate] > 0.5f) ? std::floor (step16 / 2) : step16) % 16 + 16) % 16;
        const float gTarget = kGates[pat][idx] == '1' ? 1.0f : 1.0f - p[pGdepth];
        gateG += (gTarget - gateG) * gateK;
        const float pg = pumpG * gateG;
        tape[0] *= pg; tape[1] *= pg;

        // MASSIVE: 8-line feedback delay network with a Householder mix
        float outs[8], sum = 0;
        const float feed = (tape[0] + tape[1]) * 0.5f * 0.35f;
        for (int k = 0; k < 8; ++k)
        {
            auto& l = mLines[(size_t) k];
            l.lfoPh += kMLfo[k] * mRate / fs; if (l.lfoPh > 1) l.lfoPh -= 1;
            const double base = kMBase[k] * msize;
            const double mod = mWarp * juce::jmin (0.012, base * 0.06) * std::sin (tw * l.lfoPh);
            const double d = juce::jlimit (0.001, 3.9, base + mod) * fs;
            outs[k] = readDelay (l.buf, l.w, d);
            const float f = l.lp.process (outs[k], 0) * mFb;
            outs[k] = f; sum += f;
        }
        float wetL = 0, wetR = 0;
        for (int k = 0; k < 8; ++k)
        {
            auto& l = mLines[(size_t) k];
            float v = feed + outs[k] + mHouse * sum;
            if (! std::isfinite (v)) v = 0;
            l.buf[(size_t) l.w] = v;
            l.w = (l.w + 1) % (int) l.buf.size();
            const float pan = (k % 2 ? 1.0f : -1.0f) * (0.35f + 0.6f * (float) k / 7.0f);
            wetL += outs[k] * (1 - pan) * 0.5f; wetR += outs[k] * (1 + pan) * 0.5f;
        }
        GC_CHECK (wetL, 5); GC_CHECK (sum, 6);
        wetL = mLow.process (wetL, 0); wetR = mLow.process (wetR, 1);
        const float dryG = std::cos (mmix * juce::MathConstants<float>::halfPi), wetG = std::sin (mmix * juce::MathConstants<float>::halfPi) * 1.1f * 1.6f;
        float o[2] = { (tape[0] * dryG + wetL * wetG) * master, (tape[1] * dryG + wetR * wetG) * master };

        // LIMITER (-6 dB threshold, 14:1) + makeup + safety soft clip
        const float pk = juce::jmax (std::abs (o[0]), std::abs (o[1]));
        limEnv += (pk - limEnv) * (pk > limEnv ? limAtt : limRel);
        const float envDb = juce::Decibels::gainToDecibels (limEnv + 1e-9f);
        const float grDb = envDb > -6 ? (envDb + 6) * (1 - 1.0f / 14) : 0;
        const float g = juce::Decibels::decibelsToGain (-grDb);
        for (int c = 0; c < 2; ++c)
        {
            float y = o[c] * g;
            if (! std::isfinite (y)) y = 0;
            o[c] = std::tanh (y);
        }
        outL = o[0]; outR = o[1];
        juce::ignoreUnused (bpm);
    }
};
} // namespace gc
