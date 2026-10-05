# hugenumbers

Time rises. Numerals transform.

An original Flux-inspired watchface built around enormous, expressive numerals.
The two-color field rises with every second, then resets as a new asymmetric
layout arrives at the minute. Choose Western, Eastern Arabic, or Devanagari
numerals, select from twelve palettes, or create your own two-color combination.

On supported watches, the unlit display becomes a quiet black face with colored
numeral outlines and minute-only updates. The active display uses crisp white
numerals, rounded corners, and a short layout transition at each new minute.

Features:

- Rising seconds color field
- Deterministic, animated numeral layouts
- Three numeral systems
- Twelve presets and custom colors
- 12- and 24-hour time
- Low-power backlight-off presentation on Emery, Flint, and Gabbro
- Offline configuration page; no account or network service required

The screenshots in `screenshots/emery` are unframed 200 × 228 PNG files ready
for the Emery asset collection. `marketing/hugenumbers-banner-1440x720.png` is
the optional store banner. Regenerate all images with:

```sh
.venv/bin/python tools/render_store_assets.py
```

Before uploading, check the current Developer Portal preview because banner
cropping can vary between shop surfaces.
