# Third-party licenses

## Atkinson Hyperlegible Next

MotionFX embeds **Atkinson Hyperlegible Next Regular and Bold** from the
Atkinson Hyperlegible Next project:

- Copyright 2020–2024 The Atkinson Hyperlegible Next Project Authors
- Project: `googlefonts/atkinson-hyperlegible-next`
- License: SIL Open Font License, Version 1.1

The font is bundled and embedded under the terms of the SIL Open Font License
1.1. The complete license is also fetched from the upstream project and
embedded in the MotionFX binary-data target during the build.

The SIL OFL permits the font software to be used, studied, copied, merged,
embedded, modified, redistributed, and bundled with software, provided that
the font is not sold by itself and the copyright and license notices remain
available.

## Retro Lab research references

MotionFX Build 10 contains original DSP code. No source code from the projects
below is included or linked into the plugin. They were reviewed only as public
technical references:

- **Lytrix/EMU-SP1200** — public SP-1200 BOM, schematic notes and digital-path
  references, including the documented 12-bit path and 26.04 kHz sample clock.
- **Physical and Behavioral Circuit Modeling of the SP-12 Sampler** — public
  circuit-model research describing sampling, aliasing and reconstruction
  filtering behaviour.
- **CHOW Tape Model / AnalogTapeModel** — public physical-model research for
  magnetic tape machines. The upstream implementation is GPLv3; MotionFX does
  not copy or link its source.
- **noisereduce** and public spectral-gating descriptions — conceptual reference
  for adaptive frequency-dependent noise gating. MotionFX implements its own
  lightweight real-time high-band gate.

The labels **B-style** and **C-style** describe original companding-inspired
workflows. They are not licensed or certified Dolby implementations and no
Dolby trademark is used as a product mode name.
