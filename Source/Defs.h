#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <vector>

// ============================================================================
//  GrainChop shared definitions: parameters (in Push bank order), music theory,
//  sources and presets.
// ============================================================================

namespace gc
{
static const char* kNoteNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

struct ScaleDef { const char* name; std::vector<int> steps; };
static const std::vector<ScaleDef> kScales = {
    { "Major",        { 0, 2, 4, 5, 7, 9, 11 } },
    { "Minor",        { 0, 2, 3, 5, 7, 8, 10 } },
    { "Dorian",       { 0, 2, 3, 5, 7, 9, 10 } },
    { "Mixolydian",   { 0, 2, 4, 5, 7, 9, 10 } },
    { "Minor penta",  { 0, 3, 5, 7, 10 } },
    { "Harmonic min", { 0, 2, 3, 5, 7, 8, 11 } },
    { "Phrygian",     { 0, 1, 3, 5, 7, 8, 10 } },
    { "Phrygian dom", { 0, 1, 4, 5, 7, 8, 10 } },
};

// degree-stacked chords use `deg`, rock/metal shapes use semitone offsets snapped into the key
struct ChordDef { const char* name; std::vector<int> deg; std::vector<int> semis; const char* suffix; };
static const std::vector<ChordDef> kChords = {
    { "Single",  { 0 }, {}, "" },
    { "Triad",   { 0, 2, 4 }, {}, "" },
    { "7th",     { 0, 2, 4, 6 }, {}, "" },
    { "9th",     { 0, 2, 4, 6, 8 }, {}, "" },
    { "Power",   {}, { 0, 7, 12 }, "5" },
    { "Drop",    {}, { -12, 0, 7 }, "5" },
    { "Sus4",    {}, { 0, 5, 7, 12 }, "sus4" },
    { "Sus2",    {}, { 0, 2, 7, 12 }, "sus2" },
    { "Octaves", {}, { -12, 0, 12 }, " 8va" },
    { "Quartal", {}, { 0, 5, 10, 15 }, " 4ths" },
};

static const char* kSourceNames[16] = { "Soul ooh", "Soul ah", "Choir", "Rhodes", "Strings", "Supersaw", "Glass bell", "Industrial",
                                        "Falsetto ee", "Breathy", "Wurli", "Organ", "Flute", "Brass", "808 tone", "Fuzz guitar" };
static constexpr int kNumSources = 16;
static constexpr int kSrcRandom = 16;
inline int sourceRoot (int i) { return i == 14 ? 36 : 60; }
// source layout inside sources.wav (seconds)
inline double sliceStart (int i) { return i * 1.8 + 0.02; }
inline double sliceEnd   (int i) { return i * 1.8 + 1.70; }

static const double kRateBeats[7] = { 0, 1, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
static const char* kGates[6] = { "1111111111111111", "1001001010010010", "1111111111110000",
                                 "1101101111011010", "1111111100001100", "1010101010101010" };
static const float kVowels[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };

// ---------------------------------------------------------------- parameters
enum class Fmt { Pct, Ms, Sec, Hz, Semi, Cents, Db, Bpm, Mult, Bits, Oct, Plain, Shape, Vowel, Density, MassMs, Choice, Bool };

struct ParamDef
{
    const char* id; const char* name;
    float min, max, def, curve; // curve: value = min + (max-min) * n^curve   (1 = linear)
    Fmt fmt; bool integer; const char* choices;
};

// 9 pages x 8 = 72 params. Order matters: Live puts them on Push in banks of 8, in this order.
static const char* kPageNames[9] = { "NOTE", "GRAIN", "COLOR", "REPEAT", "DIRT", "SPACE", "TAPE", "MASSIVE", "" };
static constexpr int kNumPages = 8;
static const juce::uint32 kPageColours[8] = { 0xffff6b4a, 0xff3fd7e6, 0xff3fd7e6, 0xffb8f04a, 0xffff3a5c, 0xffa98bff, 0xffe8b45a, 0xff6f8dff };

static const ParamDef kParams[] = {
    // NOTE
    { "key",     "Key",         0, 11, 2, 1, Fmt::Choice, true, "C|C#|D|Eb|E|F|F#|G|Ab|A|Bb|B" },
    { "scale",   "Scale",       0, 7, 1, 1, Fmt::Choice, true, "Major|Minor|Dorian|Mixolydian|Minor penta|Harmonic min|Phrygian|Phrygian dom" },
    { "oct",     "Octave",      1, 5, 3, 1, Fmt::Oct, true, nullptr },
    { "chord",   "Chord",       0, 9, 0, 1, Fmt::Choice, true, "Single|Triad|7th|9th|Power|Drop|Sus4|Sus2|Octaves|Quartal" },
    { "attack",  "Attack",      0.005f, 2, 0.08f, 2.5f, Fmt::Sec, false, nullptr },
    { "release", "Release",     0.05f, 4, 0.9f, 2, Fmt::Sec, false, nullptr },
    { "src",     "Source",      0, 16, 0, 1, Fmt::Choice, true, "Soul ooh|Soul ah|Choir|Rhodes|Strings|Supersaw|Glass bell|Industrial|Falsetto ee|Breathy|Wurli|Organ|Flute|Brass|808 tone|Fuzz guitar|Random" },
    { "tune",    "Src Tune",    -12, 12, 0, 1, Fmt::Semi, true, nullptr },
    // GRAIN
    { "grains",  "Grains",      2, 100, 28, 1.6f, Fmt::Plain, false, nullptr },
    { "size",    "Grain Size",  10, 500, 140, 2, Fmt::Ms, false, nullptr },
    { "link",    "Size Link",   0, 1, 0.5f, 1, Fmt::Pct, false, nullptr },
    { "rnd",     "Random",      0, 1, 0.3f, 1, Fmt::Pct, false, nullptr },
    { "spray",   "Spray",       0, 1, 0.12f, 1, Fmt::Pct, false, nullptr },
    { "pos",     "Position",    0, 1, 0.4f, 1, Fmt::Pct, false, nullptr },
    { "drift",   "Drift",       0, 1, 0.25f, 1, Fmt::Pct, false, nullptr },
    { "shape",   "Env Shape",   0, 1, 0.15f, 1, Fmt::Shape, false, nullptr },
    // COLOR
    { "detune",  "Detune",      0, 50, 8, 1, Fmt::Cents, false, nullptr },
    { "rev",     "Reverse",     0, 1, 0.1f, 1, Fmt::Pct, false, nullptr },
    { "width",   "Width",       0, 1, 0.5f, 1, Fmt::Pct, false, nullptr },
    { "octup",   "Oct Up",      0, 1, 0.08f, 1, Fmt::Pct, false, nullptr },
    { "octdn",   "Oct Down",    0, 1, 0, 1, Fmt::Pct, false, nullptr },
    { "input",   "Input",       0, 1, 0, 1, Fmt::Choice, true, "Snap to key|Pads C1+" },
    { "hold",    "Hold",        0, 1, 0, 1, Fmt::Bool, true, nullptr },
    { "master",  "Master",      0, 1.2f, 0.9f, 1, Fmt::Pct, false, nullptr },
    // REPEAT
    { "bpm",     "Tempo",       60, 180, 90, 1, Fmt::Bpm, true, nullptr },
    { "sync",    "Host Sync",   0, 1, 1, 1, Fmt::Bool, true, nullptr },
    { "rate",    "Repeat",      0, 6, 0, 1, Fmt::Choice, true, "Off|1/4|1/8|1/8T|1/16|1/16T|1/32" },
    { "swing",   "Swing",       50, 75, 58, 1, Fmt::Plain, false, nullptr },
    { "gate",    "Rpt Gate",    10, 100, 60, 1, Fmt::Plain, false, nullptr },
    { "human",   "Humanize",    0, 1, 0.15f, 1, Fmt::Pct, false, nullptr },
    { "velo",    "Velocity",    0, 1, 0.5f, 1, Fmt::Pct, false, nullptr },
    { "perpad",  "Per Pad Src", 0, 1, 0, 1, Fmt::Bool, true, nullptr },
    // DIRT
    { "dmix",    "Dirt Mix",    0, 1, 0.12f, 1, Fmt::Pct, false, nullptr },
    { "drive",   "Black Mass",  0, 1, 0.45f, 1, Fmt::Pct, false, nullptr },
    { "tone",    "Fuzz Tone",   200, 12000, 4800, 3, Fmt::Hz, false, nullptr },
    { "mass",    "Sub Mass",    0, 18, 6, 1, Fmt::Db, false, nullptr },
    { "bits",    "Crush",       2, 16, 8, 1, Fmt::Bits, true, nullptr },
    { "vowel",   "Vowel",       0, 4, 0, 1, Fmt::Vowel, false, nullptr },
    { "vmix",    "Vowel Mix",   0, 1, 0.35f, 1, Fmt::Pct, false, nullptr },
    { "dlevel",  "Dirt Level",  0, 1.5f, 0.9f, 1, Fmt::Pct, false, nullptr },
    // SPACE
    { "gmix",    "Glass Mix",   0, 1, 0.35f, 1, Fmt::Pct, false, nullptr },
    { "decay",   "Glass Size",  0.5f, 8, 3.5f, 1, Fmt::Sec, false, nullptr },
    { "shimmer", "Shimmer",     0, 1, 0.35f, 1, Fmt::Pct, false, nullptr },
    { "pump",    "Pump",        0, 1, 0, 1, Fmt::Pct, false, nullptr },
    { "wobble",  "Wobble",      0, 1, 0, 1, Fmt::Pct, false, nullptr },
    { "gpat",    "Silence",     0, 5, 0, 1, Fmt::Choice, true, "Off|Stab|Half|Trap|Yeezus|Stutter" },
    { "grate",   "Gate Rate",   0, 1, 0, 1, Fmt::Choice, true, "1/16|1/8" },
    { "gdepth",  "Gate Depth",  0, 1, 1, 1, Fmt::Pct, false, nullptr },
    // TAPE
    { "wow",     "Wow",         0, 1, 0.15f, 1, Fmt::Pct, false, nullptr },
    { "wrate",   "Wow Rate",    0.1f, 3, 0.55f, 2, Fmt::Hz, false, nullptr },
    { "flutter", "Flutter",     0, 1, 0.15f, 1, Fmt::Pct, false, nullptr },
    { "darken",  "Darken",      0, 1, 0.3f, 1, Fmt::Pct, false, nullptr },
    { "lowcut",  "Low Cut",     20, 400, 30, 2, Fmt::Hz, false, nullptr },
    { "sat",     "Tape Sat",    0, 1, 0.25f, 1, Fmt::Pct, false, nullptr },
    { "hiss",    "Hiss",        0, 1, 0.12f, 1, Fmt::Pct, false, nullptr },
    { "crackle", "Crackle",     0, 1, 0.1f, 1, Fmt::Pct, false, nullptr },
    // MASSIVE
    { "mmix",    "Massive Mix", 0, 1, 0, 1, Fmt::Pct, false, nullptr },
    { "msize",   "Delay",       0.3f, 12, 3, 2.2f, Fmt::MassMs, false, nullptr },
    { "mfb",     "Feedback",    0, 1, 0.8f, 1, Fmt::Pct, false, nullptr },
    { "mdens",   "Density",     0, 1, 0.85f, 1, Fmt::Density, false, nullptr },
    { "mdamp",   "Damping",     0, 1, 0.35f, 1, Fmt::Pct, false, nullptr },
    { "mwarp",   "Warp",        0, 1, 0.4f, 1, Fmt::Pct, false, nullptr },
    { "mrate",   "Mod Rate",    0.05f, 4, 1, 2, Fmt::Mult, false, nullptr },
    { "mlow",    "Wet Low Cut", 20, 800, 150, 2, Fmt::Hz, false, nullptr },
};
static constexpr int kNumParams = (int) (sizeof (kParams) / sizeof (kParams[0]));
static_assert (kNumParams == 64, "8 pages of 8");

enum P : int {
    pKey, pScale, pOct, pChord, pAttack, pRelease, pSrc, pTune,
    pGrains, pSize, pLink, pRnd, pSpray, pPos, pDrift, pShape,
    pDetune, pRev, pWidth, pOctUp, pOctDn, pInput, pHold, pMaster,
    pBpm, pSync, pRate, pSwing, pGate, pHuman, pVelo, pPerPad,
    pDmix, pDrive, pTone, pMass, pBits, pVowel, pVmix, pDlevel,
    pGmix, pDecay, pShimmer, pPump, pWobble, pGpat, pGrate, pGdepth,
    pWow, pWrate, pFlutter, pDarken, pLowcut, pSat, pHiss, pCrackle,
    pMmix, pMsize, pMfb, pMdens, pMdamp, pMwarp, pMrate, pMlow
};

inline juce::String formatValue (const ParamDef& d, float v)
{
    auto pct = [] (float x) { return juce::String (juce::roundToInt (x * 100)) + "%"; };
    switch (d.fmt)
    {
        case Fmt::Pct:   return pct (v);
        case Fmt::Ms:    return juce::String (juce::roundToInt (v)) + " ms";
        case Fmt::Sec:   return v < 1 ? juce::String (juce::roundToInt (v * 1000)) + " ms" : juce::String (v, 2) + " s";
        case Fmt::Hz:    return v >= 1000 ? juce::String (v / 1000, 1) + "k" : (v < 10 ? juce::String (v, 2) + " Hz" : juce::String (juce::roundToInt (v)) + " Hz");
        case Fmt::Semi:  return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v)) + " st";
        case Fmt::Cents: return juce::String::fromUTF8 ("\xc2\xb1") + juce::String (juce::roundToInt (v)) + " ct";
        case Fmt::Db:    return "+" + juce::String (v, 1) + " dB";
        case Fmt::Bpm:   return juce::String (juce::roundToInt (v)) + " bpm";
        case Fmt::Mult:  return juce::String (v, 2) + "x";
        case Fmt::Bits:  return juce::String (juce::roundToInt (v)) + " bit";
        case Fmt::Oct:   return "Oct " + juce::String (juce::roundToInt (v));
        case Fmt::Plain: return juce::String (juce::roundToInt (v)) + (juce::String (d.id) == "grains" ? "/s" : "%");
        case Fmt::Shape: return v < 0.33f ? "Soft" : v < 0.66f ? "Mid" : "Perc";
        case Fmt::Vowel: { const char* L = "AEIOU"; int i = (int) std::floor (v); float f = v - (float) i; int j = juce::jmin (4, i + 1);
                           if (f < 0.15f) return juce::String::charToString (L[i]);
                           if (f > 0.85f) return juce::String::charToString (L[j]);
                           return juce::String::charToString (L[i]) + ">" + juce::String::charToString (L[j]); }
        case Fmt::Density: return v < 0.15f ? "Echo" : v > 0.85f ? "Wash" : pct (v);
        case Fmt::MassMs: return juce::String (juce::roundToInt (0.0437f * v * 1000)) + " ms";
        case Fmt::Bool:  return v > 0.5f ? "On" : "Off";
        case Fmt::Choice: return juce::StringArray::fromTokens (d.choices, "|", "")[juce::roundToInt (v)];
    }
    return juce::String (v);
}

inline juce::NormalisableRange<float> makeRange (const ParamDef& d)
{
    const float mn = d.min, mx = d.max, c = d.curve;
    juce::NormalisableRange<float> r (mn, mx,
        [mn, mx, c] (float, float, float n) { return mn + (mx - mn) * std::pow (juce::jlimit (0.0f, 1.0f, n), c); },
        [mn, mx, c] (float, float, float v) { return std::pow (juce::jlimit (0.0f, 1.0f, (v - mn) / (mx - mn)), 1.0f / c); },
        [mn, mx, integer = d.integer] (float, float, float v) { v = juce::jlimit (mn, mx, v); return integer ? std::round (v) : v; });
    return r;
}

// ---------------------------------------------------------------- music
struct Music
{
    int key = 2, scale = 1, oct = 3, chord = 0;
    const std::vector<int>& steps() const { return kScales[(size_t) scale].steps; }
    int baseMidi() const { return (oct + 1) * 12 + key; }
    int degMidi (int d) const
    {
        const auto& s = steps(); const int n = (int) s.size();
        const int o = (int) std::floor ((double) d / n); const int r = d - o * n;
        return baseMidi() + o * 12 + s[(size_t) r];
    }
    bool inScale (int m) const
    {
        const int pc = ((m - key) % 12 + 12) % 12;
        for (int x : steps()) if (x == pc) return true;
        return false;
    }
    int snap (int m) const { for (int d = 0; d < 12; ++d) if (inScale (m - d)) return m - d; return m; }
    // degree index of an in-scale note relative to baseMidi
    int degreeOf (int m) const
    {
        m = snap (m);
        for (int d = -40; d < 60; ++d) if (degMidi (d) == m) return d;
        return 0;
    }
    std::vector<int> chordFromDegree (int d) const
    {
        const auto& c = kChords[(size_t) chord];
        std::vector<int> out;
        if (! c.deg.empty()) for (int k : c.deg) out.push_back (degMidi (d + k));
        else { const int r = degMidi (d); for (int x : c.semis) out.push_back (snap (r + x)); }
        return out;
    }
    static juce::String noteName (int m) { return juce::String (kNoteNames[((m % 12) + 12) % 12]) + juce::String ((int) std::floor (m / 12.0) - 1); }
    static juce::String chordName (const std::vector<int>& ns)
    {
        if (ns.size() == 1) return noteName (ns[0]);
        std::vector<int> r; for (int n : ns) r.push_back (n - ns[0]);
        juce::String q = r[1] == 4 ? (r[2] == 8 ? "+" : "") : r[1] == 3 ? (r[2] == 6 ? "dim" : "m") : "sus";
        if (ns.size() >= 4)
        {
            const int s7 = r[3];
            if (s7 == 11) q += (q == "m" ? "(maj7)" : "maj7");
            else if (s7 == 10) q = (q == "dim" ? juce::String ("m7b5") : q + "7");
            else if (s7 == 9) q += "6";
            if (ns.size() >= 5)
            {
                const int n9 = r[4] - 12;
                if (n9 == 2 && q.endsWith ("7") && ! q.contains ("b5")) q = q.dropLastCharacters (1) + "9";
                else q += n9 == 1 ? "(b9)" : n9 == 3 ? "(#9)" : "(9)";
            }
        }
        return juce::String (kNoteNames[ns[0] % 12]) + q;
    }
    juce::String padLabel (int i) const
    {
        const auto& c = kChords[(size_t) chord];
        auto ns = chordFromDegree (i);
        if (! c.deg.empty()) return c.deg.size() > 1 ? chordName (ns) : noteName (ns[0]);
        const int r = degMidi (i); bool alt = false;
        for (size_t k = 0; k < c.semis.size(); ++k) if (r + c.semis[k] != ns[k]) alt = true;
        const juce::String n (c.name);
        return juce::String (kNoteNames[r % 12]) + (alt ? (n == "Power" || n == "Drop" ? juce::String ("b5") : juce::String (c.suffix) + "*") : juce::String (c.suffix));
    }
};

// ---------------------------------------------------------------- presets
struct PresetDef { const char* category; const char* name; const char* values; };
#include "Presets.inc"
static constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
// params a preset never touches (your key, tempo and level stay put while browsing)
inline bool presetKeeps (int i) { return i == pKey || i == pOct || i == pBpm || i == pMaster || i == pPerPad || i == pInput || i == pSync || i == pHold || i == pVelo; }
} // namespace gc
