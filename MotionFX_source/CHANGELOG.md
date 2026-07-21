# MotionFX changelog

## 0.3.0 — Block 3

- Matched the preset-name control height to the other header buttons.
- Removed the redundant hamburger preset-menu button.
- Fixed the dragged effect tab changing its displayed identity while crossing another slot.
- Enlarged modulation-source selectors and their popup text for readability.
- Made Init start with every effect module and hidden sync toggle disabled.
- Persisted the loaded preset identity in DAW project/session state.
- Added an asterisk when the current parameters differ from the loaded or saved preset.
- Limited knob value displays to at most three decimal places.
- Added double-click direct value entry on knobs.
- Added a scrollable About/resources window by clicking the MOTIONFX title.
- Added About, changelog and preset-folder shortcuts to the Options menu.
- Added descriptive tooltips to the header controls.
- Updated GitHub Actions to Node.js 24-compatible action versions.
- Added automated checks for clean Init state and preset-name/modified-state restoration.
- Updated the CMake/VST3 project version to `0.3.0` for Build 3.

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

## 0.1.0 — Block 1

- Pause DSP when a host explicitly reports that its transport is stopped.
  - The dry input is left untouched.
  - Modulators, stutter capture and time-based/noise tails no longer continue in the background.
  - DSP state is reset once when playback transitions from running to stopped.
  - Hosts that expose no usable playhead continue to process normally.
- Preserve the selected effect identity when effect tabs are drag-reordered.
- Rename the master toggle from `MATCH` to `GAIN MATCH`.
