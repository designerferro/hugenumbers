# Offline font sources

These SIL OFL fonts are used only by the generator. None is in the Pebble resource manifest.

- RobotoFlex.ttf: https://github.com/googlefonts/roboto-flex (Google Fonts distribution, `ofl/robotoflex`).
- NotoSansArabic.ttf: https://github.com/google/fonts/tree/main/ofl/notosansarabic
- NotoSansDevanagari.ttf: https://github.com/google/fonts/tree/main/ofl/notosansdevanagari

The adjacent `*-OFL.txt` files contain each font's license and copyright notices.
`resources/numerals/manifest.json` records the SHA-256 of each exact source font.
Roboto Flex lacks the requested Eastern Arabic and Devanagari codepoints; the Noto
sources supply actual native numerals rather than substituting Western glyphs.
