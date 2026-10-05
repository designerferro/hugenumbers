# hugenumbers

A Flux-inspired Pebble watchface with oversized, independently deformed HH/MM
numerals and a rising seconds colour field. This is an original implementation
using open-source fonts, not Apple's artwork or font assets.

The lower region grows by `seconds / 60.0`: empty at `:00`, almost full at `:59`,
then resets at the next minute. Numerals remain white across both color fields,
and the rectangular face is clipped to rounded outer corners. Hours sit above
minutes, respecting the watch's 12/24-hour preference.
Each minute deterministically changes column widths and the two column height
splits. An eight-frame, 400 ms geometry transition moves into the new layout;
the new digits and bitmap width masters take effect at the minute boundary.

On Emery, Flint, and Gabbro, turning the backlight off switches to a static,
inverted presentation: the background becomes black and only numeral outlines
remain. Dark scheme colors are lifted to a lighter shade of the same hue so the
outlines retain contrast. The watchface also drops to
minute-only updates while unlit. Lighting the display restores the animated
two-color seconds field and second-by-second updates.

In the Pebble phone app settings, choose Western (`0123456789`), Eastern Arabic
(`٠١٢٣٤٥٦٧٨٩`), or Devanagari (`०१२३४५६७८९`) numerals, then choose a preset or
custom colour scheme. The expanded palette includes eight blue-tone pairs, and the Custom
option provides separate pickers for the upper field and rising seconds field.
Select **Custom** in the Colour scheme menu before using either color picker;
preset schemes ignore the picker values.
Pebble maps picker values to its supported hardware palette. Settings persist across restarts.
Old font-colour settings are superseded.

The watch accepts both numeric AppMessages and the string values emitted by
different Clay/WebView versions, including hexadecimal custom colors.

The **When animation stops** setting controls the idle presentation:

- **Inverted outline** uses a black background and light scheme-colored outlines.
- **Color field outline** freezes the color boundary and outlines the numerals.
- **Freeze normal face** freezes the boundary and keeps solid white numerals.

All modes continue updating the hour and minute without animating the seconds field.

The presets are Mint / violet, Lemon / midnight, Apricot / burgundy, Black /
white, Ice / cobalt, Sky / navy, Azure / royal blue, Cyan / midnight,
Periwinkle / indigo, Powder blue / ocean, Electric / deep blue, and Teal / navy.
The vendored MIT-licensed Clay bundle supplies the offline settings page.

## Build and install

Generated resources are checked in. Normal builds need the Pebble SDK, not Python
font libraries. All original targets and `wscript` build rules are preserved.

```sh
pebble build
pebble install --emulator emery
pebble install --phone <ip>
```

Output: `build/hugenumbers.pbw`. Targets: aplite, basalt, chalk, diorite, emery,
flint, and gabbro.

## Regenerate numeral assets

```sh
python3 -m venv .venv
.venv/bin/pip install -r tools/requirements.txt
.venv/bin/python tools/generate_numerals.py
.venv/bin/python tools/check_numerals.py
pebble build
```

The pipeline uses bundled source fonts under `tools/fonts`:

1. Subset to the ten required digits before expensive instancing.
2. Instantiate 16 `wdth`/`wght` combinations with fontTools (Roboto Flex also uses
   its large optical-size master).
3. Rasterize the instantiated outlines with Pillow/FreeType at high resolution.
4. Apply independent horizontal/vertical transformations beyond normal `wdth`
   limits, from 18 to 108 pixels wide at 80 pixels high; threshold to 1-bit masks.
   Lighter narrow masters help keep counters open. Eastern Arabic zero remains a dot.
5. Encode masks losslessly as repeated scanline runs and write three raw Pebble
   resources plus `src/c/numeral_assets.h`. Only numeral masks enter the PBW.

[Roboto Flex](https://github.com/googlefonts/roboto-flex) supplies Western digits.
It does **not** contain Eastern Arabic or Devanagari digits. Licensed Noto Sans
Arabic and Noto Sans Devanagari variable fonts supply those scripts using the same
pipeline. Source provenance, licenses, and checksums are recorded in
`tools/fonts/README.md`, adjacent OFL files, and `resources/numerals/manifest.json`.
No source font is included in the Pebble resource manifest.

The checker validates all 480 records, resource budget, layout bounds across all
1,440 minutes, and seconds endpoints. It generates `build/previews/layouts.png`;
the generator also creates per-script width-state contact sheets there.

## Rendering and hardware limits

- Four fixed mask buffers use 1,792 bytes. Masks load only for minute/configuration
  changes. Repeated scanline runs render as filled rectangles, split at the colour
  boundary. There is no runtime font/vector processing, colour-specific mask
  duplication, or temporary framebuffer. Rectangle coordinates are scaled with
  integer arithmetic during the brief transition and for each screen's size.
- Resources total 127,042 bytes including SDK pack overhead on Aplite, below its
  131,072-byte limit. Aplite's static app footprint is approximately 4.5 KB, leaving
  about 20 KB for heap/SDK objects. The validator guards the resource budget.
- Aplite, Diorite, and Flint use black/white inversion. Both color fields retain
  visible numeral outlines. Colour watches use Pebble's
  limited palette. The fill updates once per second to limit CPU/battery use;
  it is not a continuous 60 fps animation. Per-second updates cost more battery
  than the previous minute-only face; physical-device battery life is unmeasured.
- Round displays use a 15% inset to keep the numerals inside the circle, leaving
  more unused space than rectangular displays. The field still fills the screen.
- Extreme width changes are deliberately stylized; bitmap quantization is visible,
  especially on larger screens. Width/height changes animate, but glyph outlines
  do not morph between masters. Arabic zero intentionally leaves space around its dot.
- SDK 4.33.1's generated linker script emits a `LOAD segment with RWX permissions`
  warning with its current GCC linker. The project does not override the SDK's
  memory layout or suppress the warning. There are no C compiler warnings.
- The SDK exposes backlight state and transition events only on Emery, Flint, and
  Gabbro. Aplite, Basalt, Chalk, and Diorite therefore show the static outline by
  default and show the animated seconds field for 15 seconds after a shake/tap.

## Validation

Built successfully with SDK 4.33.1 for all seven targets. Emulator screenshots
confirmed Western, Eastern Arabic, and Devanagari rendering and palette changes
on Emery, and monochrome rendering plus minute rollover on Aplite. Screenshots
are under `build/previews/`. Other targets were compiled but not visually tested
in emulators; physical hardware and battery use have not been measured.

## Store publishing

`store/listing.md` contains ready-to-edit shop copy. The three unframed Emery
screenshots under `store/screenshots/emery` use the platform's native 200 × 228
resolution, and `store/marketing/hugenumbers-banner-1440x720.png` is an optional
marketing banner. Regenerate them after visual changes with:

```sh
.venv/bin/python tools/render_store_assets.py
```

The same command creates `resources/images/menu_icon.png`, which is embedded in
the PBW as its launcher icon so Pebble Core can show art for a sideloaded face.

The current Pebble publishing CLI accepts screenshot paths whose filenames begin
with the platform name; the banner is uploaded through the Developer Portal.

## Files

- `src/c/hugenumbers.c`: seconds, deterministic layout, mask rendering, persistence.
- `src/c/numeral_assets.h`: generated mask capacity/state constants.
- `src/pkjs/config.json`: numeral and palette settings.
- `resources/numerals/`: generated mask resources and provenance manifest.
- `tools/generate_numerals.py`, `tools/check_numerals.py`: generation and validation.
- `tools/render_store_assets.py`, `store/`: reproducible shop images and listing copy.
- `tools/fonts/`, `tools/requirements.txt`: offline sources, licenses, dependencies.
- `package.json`: resource manifest and AppMessage keys.

SDK documentation: <https://developer.repebble.com>

## Development

Developed with assistance from OpenAI Codex.