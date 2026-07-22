# MotionFX changelog

## 0.5.0 - Block 5

- Added a dedicated Filter category with Low Pass, High Pass, Band Pass, Notch, Peak and Comb modes.
- Added selectable 12, 24, 36 and 48 dB/oct filter slopes.
- Added live input/output signal traces behind each effect modulation display.
- Reworked knob interaction: double-click the rotary body to return to zero; double-click the value or label to type a value.
- Fixed floating-point displays to two decimals and added contextual units such as %, dB, Hz, s and ms.
- Kept the original visible effect control names, using tooltips for extra context; mode-dependent controls still switch between delay TIME/FEEDBACK and reverb SIZE/DECAY.
- Added free delay timing displayed as Hz or seconds plus tempo-synchronised musical divisions.
- Removed the Stutter ALT 1/8 helper and shortened its instruction text.
- Removed the redundant Open Changelog menu item.
- Prevented a dragged tab from looking like the currently selected effect.

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
