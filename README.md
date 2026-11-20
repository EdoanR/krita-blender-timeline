# Krita Timeline

Krita Timeline is a source fork of Krita 5.2.5 with Blender-inspired animation
timeline editing. It uses Krita's existing layers, keyframe channels, undo system,
and KRA document format.

## Timeline features

- Summary keys represent keyframes across the visible timeline layers. Click to
  select a column, Shift-click to select a range, and Ctrl-click to toggle columns.
- Drag summary keys to move a selection. Summary moves can replace keys at the
  destination, with Undo restoring the replaced keys.
- Grab and scale keys with G and S, mouse movement, or numeric input. Enter
  confirms and Esc cancels. Shift gives finer movement and Ctrl enables snapping.
- Playback Start/End and an optional preview range appear in the bottom row.
  Preview bounds and the enabled state are saved in KRA files.
- Shift-click any range field to use the current frame. Crossing the opposite
  boundary expands the range to keep it valid.
- Clicking a frame cell activates that cell's layer, including empty cells.
- Use the wheel to zoom around the pointer. The footer provides a zoom slider and
  a Fit button for the playback range, enabled preview range, and timeline keys.
- Hold Ctrl+Space and drag vertically over the grid, summary, or ruler to zoom
  with a pen or mouse. Drag zoom follows the canvas inversion preference.
- Hold Space and drag to pan. Playback uses Krita's transport controls or a
  shortcut assigned in Keyboard Shortcuts.

The timeline supports frames 0 through 9999. The frame grid has a minimum width
of one pixel per frame, so very long ranges can exceed the viewport at minimum
zoom. Summary scaling rejects ambiguous destinations where selected keys would
collapse onto the same frame.

## Build and tests

Use the standard Krita 5.2.5 development dependencies and build process described
in the [Krita build documentation](https://docs.krita.org/en/untranslatable_pages/building_krita.html).

The timeline regression tests are part of Krita's existing animation test suite.
With testing enabled in a configured development build, run:

    cmake --build build --target timeline_model_test
    ctest --test-dir build -R plugins-dockers-animation-timeline_model_test --output-on-failure

The latest local Windows validation passed 16 QtTest cases covering transforms,
overwrite undo, preview persistence, layer activation, range-field input, wheel
zoom, and the pen/mouse zoom gesture. A fresh complete build of this cleaned
repository and other platforms still need validation.

## AI disclosure

The timeline changes in this fork were developed with substantial assistance
from OpenAI Codex, including AI-generated code, tests, and documentation. The
project maintainer directed the features and tested the behavior interactively.
This disclosure applies to this fork's changes; the original Krita source retains
its existing authorship and copyright notices.

## Upstream

The starting tree is a source snapshot of
[Krita v5.2.5](https://invent.kde.org/graphics/krita/-/tree/v5.2.5),
upstream commit 454726af046a0fdd7ccae8ba2a7cfde18a344c7c.
This repository records the snapshot import and subsequent timeline changes.
Krita's earlier commit history is available in the
[upstream repository](https://invent.kde.org/graphics/krita).

For the full Krita user manual, visit [docs.krita.org](https://docs.krita.org/).
Contributions to this fork can be submitted through its GitHub pull requests.
Follow the upstream coding guidance in HACKING.

## License

Krita's original copyright notices and license files are retained.
See [COPYING](COPYING) and the license notices in individual source files.
