# MotionFX changelog

## 1.1.0-dev — Block 1

- Pause DSP when a host explicitly reports that its transport is stopped.
  - The dry input is left untouched.
  - Modulators, stutter capture and time-based/noise tails no longer continue in the background.
  - DSP state is reset once when playback transitions from running to stopped.
  - Hosts that expose no usable playhead continue to process normally.
- Preserve the selected effect identity when effect tabs are drag-reordered.
- Rename the master toggle from `MATCH` to `GAIN MATCH`.

## 0.2.0 — Block 2
- The preset name in the header is now the preset-browser control.
- Added an explicit Init preset that restores the unmodified default state.
- Added recursive user preset discovery in folders and subfolders.
- Added nested Factory Presets and User Presets menus.
- Added preset saving into a selected relative folder.
- Added preset-folder creation from the preset browser and options menu.
- Added preset-browser access from the options menu.
- Made JUCE acquisition portable through JUCE_DIR, a sibling checkout, or CMake FetchContent.
- Added GitHub Actions builds for Windows/MSVC and Linux, including automated audio and GUI tests.
