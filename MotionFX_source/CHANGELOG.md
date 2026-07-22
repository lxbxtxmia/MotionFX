# MotionFX changelog

## 0.8.1 - UI refinement

- Corrected Undo and Redo vector icons so the curve no longer crosses the arrow head.
- Restored the MotionFX wordmark hierarchy and increased interface text readability.
- Added compact engineering notation such as 20.0 kHz while preserving precise manual entry.
- Added contextual Pan, Filter, Width, Delay and Retro values.
- Expanded Width to 0-200%, with 0% mono and 100% unchanged stereo.
- Added live modulation-range arcs and a target/range readout on primary effect knobs.
- Simplified the stereo output meter into two gradient bars.
- Applied adaptive corner radii for square and rectangular controls.

## 0.8.0 - Block 8

- Rebuilt the main header with balanced spacing and vector icon buttons.
- Added compact, perfectly circular master knobs and cleaner flat control rendering.
- Added a post-effect stereo peak meter with green below -6 dBFS, yellow from -6 to 0 dBFS, and red above 0 dBFS before the safety ceiling.
- Embedded Atkinson Hyperlegible Next Regular and Bold from the official project.
- Added persistent Dark and Light themes plus user-created `.mfxtheme` JSON files.
- Added Accessibility options for High Contrast on any theme, Reduced Motion, Enhanced Controls and Larger Text.
- Added Undo, Redo and Save keyboard shortcuts.
- Expanded GUI snapshot tests across themes, high-contrast overlays and all supported interface scales.

## 0.7.0 - Block 7

- Limited non-Comb filter resonance to a 12 dB total boost, with a gentler squared control curve.
- Made the Stutter playhead repaint continuously and made CLEAR refresh all lanes immediately.
- Reduced Stutter lane typography and improved long ToggleButton text such as GAIN MATCH and TEMPO SYNC.
- Added branching Undo/Redo history with header controls, history-point navigation and crash-recovery journaling.
- Shortened preset names after 20 characters while exposing the full name in a tooltip.
- Added preset creator, creation time, schema and MotionFX-version metadata.
- Added a compatibility error when a preset was created by a newer MotionFX version.
- Added a real TooltipWindow and compacted the header layout.

## 0.6.0 - Block 6

- Signal visualisers now return smoothly to zero when audio processing stops instead of holding the last level.
- Dragged effect tabs use a neutral ghost style and no longer look selected.
- Rebuilt Stutter as five independent, stackable lanes: Repeat, Reverse, Tape, Pitch and Gate.
- Added per-step overlapping actions through the new multi-row Stutter editor.
- Added independent -24 to +24 semitone values for every Pitch cell, with realtime dual-grain processing and adjustable grain size.
- Added left-drag painting and right-drag erasing for every Stutter lane; Pitch uses vertical semitone editing and click-to-zero.
- Removed the legacy single-action Stutter step format; old Stutter patterns are intentionally not migrated.

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
