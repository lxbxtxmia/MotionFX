# MotionFX changelog

## 0.4.0 - Block 4

- Reworked the interface hierarchy so related modulation controls are grouped together.
- Fixed rotary controls becoming tiny inside wide layout cells.
- Enlarged and unified master, effect and modulation knob sizes.
- Enlarged effect-mode selectors and their popup text.
- Limited float controls to two decimal places and retained double-click numeric entry.
- Added Hz or seconds units for free-running LFO, Motion and Sequencer rates.
- Added dotted and additional triplet tempo divisions while preserving existing preset indices.
- Made tempo-synced and free-running controls switch contextually instead of displaying conflicting values.
- Reworked Stutter repeat timing so Repeat 1/4, 1/8, 1/16 and 1/32 are absolute musical lengths.
- Added short Stutter loop-boundary crossfades plus Clear and alternating 1/8 helpers.
- Replaced ambiguous About credits with the Paom credit and human-review AI disclosure.
- Consolidated About and Changelog into one expandable window using ASCII-safe text.
- Updated the project/VST3 version to `0.4.0`.
- Registered the audio and GUI test executables with CTest so CI runs real tests.

## 0.3.0 - Block 3

- Matched the preset-name control height to the other header buttons.
- Removed the redundant hamburger preset-menu button.
- Fixed the dragged effect tab changing its displayed identity while crossing another slot.
- Enlarged modulation-source selectors and their popup text for readability.
- Made Init start with every effect module and hidden sync toggle disabled.
- Persisted the loaded preset identity in DAW project/session state.
- Added an asterisk when the current parameters differ from the loaded or saved preset.
- Added double-click direct value entry on knobs.
- Added About, changelog and preset-folder shortcuts.
- Updated GitHub Actions to Node.js 24-compatible action versions.

## 0.2.0 - Block 2

- Added the preset browser and recursive user preset folders.
- Added portable CMake/JUCE acquisition and automated Windows/Linux builds.

## 0.1.0 - Block 1

- Paused DSP when the host transport is stopped.
- Preserved selected-effect identity during effect reordering.
- Renamed the master toggle to `GAIN MATCH`.
