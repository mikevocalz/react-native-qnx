# Bundled default sans font

These are unmodified, publicly released Roboto 3.015 variable TrueType fonts.
They provide a predictable default sans face when the QNX image has no suitable
installed font. The two binary filenames are shortened locally; their contents
are unchanged. No font from the Qt evaluation or QNX SDK is included.

## Source and license

The distribution source is
[`google/fonts` at `1c627bfa375fc51cf86fabeca4f6e08a95f0aa5c`](https://github.com/google/fonts/tree/1c627bfa375fc51cf86fabeca4f6e08a95f0aa5c/ofl/roboto).
Its `METADATA.pb`, preserved here, identifies the upstream source as
[`googlefonts/roboto-classic` at `91d5d3e5b81efa04a77925cc609fdcdd7ee663d1`](https://github.com/googlefonts/roboto-classic/tree/91d5d3e5b81efa04a77925cc609fdcdd7ee663d1)
and the versioned release as Roboto 3.015.

Copyright 2011 The Roboto Project Authors
(https://github.com/googlefonts/roboto-classic).
The font files remain under the SIL Open Font License 1.1, reproduced verbatim
in [OFL.txt](OFL.txt). Repository code's MIT license does not replace that
license. Distributions containing the fonts must retain the copyright notice
and full OFL text. The CMake resource includes `OFL.txt` with the font data, and
the install rule also writes a readable copy to
`share/licenses/react-native-qnx/Roboto/OFL.txt`. The `ConsumerApp` install
component places its readable copy at `licenses/Roboto/OFL.txt` alongside the
application's installed files.

## Exact files

| Local file           | Original path under `ofl/roboto` |  Bytes | SHA-256                                                            |
| -------------------- | -------------------------------- | -----: | ------------------------------------------------------------------ |
| `Roboto-Regular.ttf` | `Roboto[wdth,wght].ttf`          | 488584 | `d7598e12c5dbef095ff8272cfc55da0250bd07fbdecbac8a530b9b277872a134` |
| `Roboto-Italic.ttf`  | `Roboto-Italic[wdth,wght].ttf`   | 530944 | `9725a847af6b460ffca162ae66d20dad48b01876137947180b42d7dcd7887182` |
| `OFL.txt`            | `OFL.txt`                        |   4394 | `061402327a96aadb0bfb694a960ed289ecd38d383e396243831ab81feb109c41` |
| `METADATA.pb`        | `METADATA.pb`                    |   1463 | `6fef19da54fb62d152f0c7b21710e6849950db7ecf852ea29511c593dad462f0` |

The font Git blob IDs are `5522a368d9072fd88c299916e61fcff369949061`
(upright) and `a122c13964daac37f1186a22603a5180d2dba5d6` (italic).
CMake verifies the font and license hashes before embedding them.

## Runtime behavior and coverage

The existing renderer target embeds the font files as Qt resources; deployment
does not depend on a separate font directory or a network request. The shared
text resolver registers them once through `QFontDatabase` and uses the actual
registered family for default/system/sans text. Explicit font families keep
priority, with this family as a fallback. If Qt cannot load the resource, the
resolver retains the platform's general/sans fallback instead of aborting the
application. Registration and family selection are shared by text measurement
and mounting.

The preserved metadata lists Latin, Cyrillic, Greek, Vietnamese, math and symbol
subsets. This is not complete coverage of all scripts; applications still need
appropriate fonts for content outside those glyphs. The fonts' weight and width
axes do not add new public React Native font APIs.
