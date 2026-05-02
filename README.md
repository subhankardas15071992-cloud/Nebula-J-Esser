# Nebula J-Esser

Nebula J-Esser is a free and open source 64-bit de-esser by Nebula Audio, licensed under AGPL v3. It is built in pure C++ with JUCE and targets VST3, AUv2 on macOS, LV2, and CLAP through the JUCE CLAP extension layer.

This is deliberately not an analog cosplay plugin. The interface is dark, flat, resizable, and software-native: spectrum nodes, colored control arcs, fast meters, direct numeric entry, A/B state switching, presets, undo/redo, soft bypass, and MIDI learn are part of the first-class workflow.

## Signal Path

- 64-bit internal DSP throughout, including float-host processing through a double precision bridge.
- 384 kHz maximum internal preparation path when the host provides that sample rate.
- Zero reported latency when lookahead is off.
- Optional 0 to 20 ms lookahead, with host latency reporting updated when enabled.
- TKEO-style instantaneous energy detection for fast sibilance discrimination.
- Absolute mode uses a fixed 3-vector voiced, unvoiced, and residual model.
- Relative mode adapts the detection context for more complex vocal textures.
- Split mode computes complementary bands with `lo = LP(x)` and `hi = x - lo`, so dry recombination is exact before attenuation.
- Wide mode uses cascaded bell processing for phase-coherent broad correction.
- 6th-order Butterworth-style TPT detection filtering.
- 3-stage smoothing for gain movement and anti-zipper behavior.
- Lock-free meter handoff through atomic packed floats.
- Analyzer handoff uses `try_lock`, so the audio thread never waits for UI drawing.

## Controls

- Detection meter, peak field, and TKEO Sharp slider.
- Annihilation meter, peak field, and Annihilation slider.
- TKEO Sharp, Annihilation, Max Frequency, Min Frequency, Cut Width, Cut Depth, Cut Slope, Mix.
- Relative and Absolute detection modes.
- Split and Wide range modes.
- Lowpass and Peak detection filter shapes.
- Filter Solo and Trigger Hear monitoring.
- Single Vocal and Allround source modes.
- Internal, External, and MIDI trigger sources.
- Stereo Link with Mid or Side behavior.
- Lookahead enable plus lookahead time.
- Input/output level and pan controls.
- Post-effects spectrum analyzer with draggable Min and Max frequency nodes.
- Preset save/load menu, Undo, Redo, A/B, MIDI Learn, and FX Bypass.

Right-click a knob or slider field to enter an exact numeric value. Click the meter peak fields to reset peak hold. Drag the bottom-right corner to resize; the plugin stores the window size with the plugin state.

## Build

Requirements:

- CMake 3.24 or newer.
- A C++20 compiler.
- JUCE 8.x, either supplied locally with `-DNJ_JUCE_PATH=/path/to/JUCE` or fetched by CMake.
- The JUCE CLAP extension layer if CLAP is enabled, either supplied locally with `-DNJ_CLAP_EXTENSIONS_PATH=/path/to/clap-juce-extensions` or fetched by CMake.
- Platform SDKs for the requested formats.

Windows x86_64:

```powershell
.\scripts\build-windows.ps1 -Config Release
```

macOS arm64:

```bash
./scripts/build-macos.sh arm64 Release
```

macOS x86_64:

```bash
./scripts/build-macos.sh x86_64 Release
```

Linux x86_64:

```bash
./scripts/build-linux.sh Release
```

Artifacts are generated under the CMake build directory in `*_artefacts` folders.

## Validation

The validation executable runs deterministic signal-derived checks:

- Null consistency and bypass null behavior.
- FFT-derived spectral balance.
- LUFS-style loudness stability.
- Transient integrity and temporal smoothness checks.
- Harmonic musicality and stereo coherence metrics.
- Golden-reference comparison against a deterministic transparent de-essing target.
- Buffer-size torture sweep.
- Per-block timing guardrails.
- Worst-case input, denormal, silence, fuzz, and sample-rate switching checks.
- Parameter automation, state reset, analyzer thread-safety, and long-run stability loops.

Run it with:

```bash
ctest --test-dir build --output-on-failure
```

## License

Nebula J-Esser is free and open source software under the GNU Affero General Public License v3.0 only. See `LICENSE`.
