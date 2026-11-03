# GrainChop

A granular chord instrument for Ableton Live and Push. Hold a note and it sustains as a cloud of grains. Stack notes into chords, and every note is locked to your key. It comes with 16 tuned sources, a Yeezus/Flume FX chain (Black Mass, Glass, Pump, Silence gate), RC-20 style tape, a Supermassive-style reverb and 39 presets.

## Install (Mac)

1. Open **Releases** on the right side of this page and download `GrainChop-mac.zip`.
2. Unzip it, then right-click **Install GrainChop.command** and choose **Open** (choose Open again if macOS asks).
3. In Ableton: **Settings → Plug-Ins**, turn on **Use VST3 Plug-in System Folders** and **Use Audio Units**, then press **Rescan**.
4. GrainChop appears under **Plug-ins → Maahy**.

## Push

The parameters are laid out in 8 banks of 8, in the same order as the plugin's pages: NOTE, GRAIN, COLOR, REPEAT, DIRT, SPACE, TAPE and MASSIVE. Push shows them bank by bank.

- **Input → Snap to key** (the default): play anything, and every note snaps into the key and scale set on the NOTE page.
- **Input → Pads C1+**: notes C1 to D#2 become 16 scale-degree pads, MPC style.
- The **mod wheel or touch strip (CC1)** moves where the grains read from.
- **Repeat, Pump, Silence and Wobble** follow Live's tempo.

## Build it yourself

```
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Every push to `main` builds the Mac version automatically (see `.github/workflows/build-mac.yml`).
