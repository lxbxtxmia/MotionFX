# MotionFX — Block 3 updater v3

This updater targets the current `main` branch layout and validates every expected source fragment before writing anything.

## Apply

Place `apply_block3.py` at the repository root, alongside `.github` and `MotionFX_source`, then run:

```powershell
py -3 .\apply_block3.py
```

or:

```powershell
python .\apply_block3.py
```

Commit and push the modified files, then let the existing GitHub Actions workflow run.

## Included

- Compact preset-name control and removal of the redundant hamburger button.
- Stable dragged-tab identity.
- Larger readable modulation-source selector and popup text.
- Init with all effect modules, hidden sync toggles, Stutter, and Gain Match disabled.
- Preset name persistence across DAW session reloads.
- `*` dirty marker when the loaded or saved preset has been edited.
- Maximum three-decimal knob displays and double-click value entry.
- Node.js 24-compatible GitHub Actions versions.
- Regression tests for Init and preset session metadata.
- Project and VST3 version updated to `0.3.0` (Build 3).
- Clickable MOTIONFX title opening a scrollable About/resources window.
- Scrollable changelog available from Options.
- Option to open the active preset folder directly.
- Header control tooltips and a modified-preset tooltip.
