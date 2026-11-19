# react-native-svg reference corpus

The corpus checks the Qt renderer (`native/qt/rnsvg`) against a second
rasteriser. It holds SVG documents ported from react-native-svg 15.16.0's own
examples and regression tests, a resvg rendering of each, and two runners that
paint the same documents through the renderer and diff the pixels.

## Layout

| Path                                                                                           | Content                                                                                                                                                                            |
| ---------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `tests/fixtures/rnsvg/svg/<name>.svg`                                                          | One document per upstream scene, e.g. `Circle-1.svg`                                                                                                                               |
| `tests/fixtures/rnsvg/golden/<name>.png`                                                       | resvg 0.50.0 rendering at 1x on white. Never edited by hand                                                                                                                        |
| `tests/fixtures/rnsvg/manifest.json`                                                           | Per fixture: upstream file, scene and line range, elements and attributes used, status, thresholds, source hash, size. Also the resvg version and the scenes that are not drawings |
| `scripts/rnsvg-reference.mjs`                                                                  | Renders the goldens, checks them, writes the real-path data                                                                                                                        |
| `tests/native/qt-rnsvg/RnsvgFixturesTest.cpp`                                                  | Host runner. Target `rnqnx_qt_rnsvg_fixtures_test`                                                                                                                                 |
| `tests/native/qt-rnsvg/RnsvgLoaderTest.cpp`                                                    | Tests of the loader and the diff. Target `rnqnx_qt_rnsvg_loader_test`                                                                                                              |
| `tests/native/qt-rnsvg/support/`                                                               | Test-only SVG loader, image diff, manifest reader                                                                                                                                  |
| `apps/rn-playground/rnsvg-corpus.js`, `tests/native/qt-rnsvg-fabric/RnsvgCorpusFabricTest.cpp` | Real-path runner                                                                                                                                                                   |

Fixture names follow the upstream file and a scene number: `Circle-2` is the
second scene of `Circle.tsx`. A fixture is a standalone 1.1/2 document with the
same elements and attributes as the scene. `Test*` fixtures come from
`apps/common/test`, `Fe*` and `FilterImage-*` from the filter examples, and
`Complex-*` from the two large examples. Fixtures marked "added" in their
`covers` text exercise a prop that the upstream examples list but do not use,
for example `strokeLinejoin` or `spreadMethod`.

Where a scene cannot be ported unchanged, the manifest `covers` text says what
changed and why. Three kinds of change occur: nested `<Svg>` views become
translated groups (react-native-svg lays them out with Yoga, which has no SVG
equivalent); markers on `circle`, `ellipse` and `rect` become the same outline as
a `path`, because resvg draws markers on path-like shapes only; and `pt` sizes on
a root become pixels, because react-native-svg reads them with `parseInt` while
resvg converts them.

Scenes that are not drawings are listed under `nonVisual` in the manifest with a
reason and the test that covers them, if any: touch handlers, animations,
`toDataURL` and the other native methods, network loading, and `ForeignObject`
(its children are React Native views).

## Statuses

| Status              | Meaning                                                                                                                                           |
| ------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| `pass`              | The rendering must be within the fixture's thresholds                                                                                             |
| `pending:<feature>` | The renderer cannot draw it yet. The runner skips it and names the feature. `pendingNote` records the measured difference and the suspected cause |
| `reference-none`    | resvg cannot render the document, `reason` says why. The runner loads and paints it and checks that nothing crashes                               |

CI should fail while any `pending:` entry remains. Set
`RNQNX_RNSVG_FAIL_ON_PENDING=1` to make the runners do that.
`RNQNX_RNSVG_RUN_PENDING=1` runs pending fixtures anyway and prints
`READY <name>` for each one that is now within its thresholds, which is the
cue to remove its status.

## The metric

Both images are compared as opaque RGB. A pixel differs when any channel is
more than 24 of 255 away. Five numbers come out, and a fixture fails when any
exceeds its limit:

| Number                                          | Default limit | Catches                                             |
| ----------------------------------------------- | ------------- | --------------------------------------------------- |
| Pixels that differ                              | 1.5 percent   | wrong colors, shifted shapes                        |
| Mean absolute difference                        | 2.0           | small differences everywhere, such as blur or gamma |
| Largest block of differing pixels (4-connected) | 400 px        | one wrong shape in a mostly right picture           |
| Pixels that match nothing within one pixel      | 0.8 percent   | the first number without edge noise                 |
| Largest block of those                          | 60 px         | the third number without edge noise                 |

The last two forgive a differing pixel when it equals some reference pixel in its
3x3 neighbourhood (or the reverse). Two rasterisers place anti-aliased edge
pixels differently, and on 1 px strokes that noise alone can reach several
percent. A missing marker, a wrong tile or a wrong hue survives the forgiveness.
The limits come from the 80 passing fixtures that need no override on commit
112fe48: their largest values were 1.47 percent, a mean of 0.95, a 47 px block,
0.72 percent and a 22 px block. A missing 10 by 10 shape leaves a 64 px block
after forgiveness, over the limit of 60. Eight more fixtures pass with an
override.

A fixture overrides a limit in `thresholds` and gives the reason in
`thresholdReason`, one line. On failure the runner writes
`<name>.actual.png` and `<name>.diff.png` into the working directory and prints
the numbers. With `RNQNX_RNSVG_CALIBRATE=1` it prints one `CAL` line per fixture,
writes both images for every fixture and never fails on the metrics.

## Fonts

Text fixtures use the DejaVu fonts in `tests/native/qt-rnsvg/fonts`. resvg runs
with `--skip-system-fonts --use-fonts-dir` on that directory,
`--font-family "DejaVu Sans"` and its default 12 px. The Qt runner registers the
same files and sets the application font to DejaVu Sans at 12 px. Text
rasterisation still differs (hinting), so the text fixtures rely on the same
limits and a failing text fixture needs a look at its diff image.

## Regenerating

```
node scripts/rnsvg-reference.mjs            # render every golden, update sha256 and size
node scripts/rnsvg-reference.mjs Circle-1   # one fixture
node scripts/rnsvg-reference.mjs --check    # fail on a missing golden, a source whose hash differs
                                            # from the manifest, or a different resvg version
```

resvg is taken from `/opt/homebrew/bin/resvg`; set `RESVG` to use another
binary. The version in the manifest is 0.50.0, and a golden rendered by another
version fails `--check`. `--check --mtime` also compares file modification times,
which only works in a working tree. To change a document, edit the `.svg`, run
the script, and commit the source, golden and manifest together.

## The loader

`support/SvgDocumentLoader` turns a document into the retained tree
(`RnsvgRootItem` and `RnsvgElementItem`) the Fabric bridge builds for the same
markup rendered with `SvgXml`. It follows react-native-svg's own code:

- the XML parser in `src/xml.tsx` (attribute names camel-cased, the `style`
  attribute split into declarations, whitespace-only text dropped);
- `propsAndStyles()`, where an explicit attribute beats the same name in `style`;
- `extractProps`, `extractFill`, `extractStroke`, `extractBrush`,
  `extractTransform`, `extractViewBox`, `extractGradient`, `extractText` and
  `extractFilter` for what reaches the native side, including `propList`;
- the element components for defaults (a Marker's `markerWidth` is 3, a Filter's
  region is -10% to 120%).

Documents with a `<style>` block go through `support/SvgCss`, which follows
`src/css/css.tsx` (the manifest marks them `css`). `feDropShadow` expands to the
five primitives the JS component renders, and the Fe* tags that 15.16.0 leaves
unimplemented produce no element, as in react-native-svg. The loader reports
what it did not mount in `LoadedDocument::skipped`.

The model-specific code is behind `support/ModelAdapter.h`. Built against the
model of commit 112fe48 (no `RnsvgBasicTypes.h`) the loader compiles without the
new element kinds and fields, which gives the baseline run the statuses record.

Bounds: nesting over 256 levels is refused, a document holds at most 500,000
elements, a data URI over 16 MiB of text or a raster over 8192 px a side is not
decoded, and the diff refuses images over 64 million pixels.

## Real path

`apps/rn-playground/rnsvg-corpus.js` draws the same documents through
react-native-svg's `SvgXml` (`SvgCss` for the `css` ones), the generated Fabric
props, the bridge and the mounting layer, in batches of at most 12 cells in a
1100 by 1000 window. `RnsvgCorpusFabricTest.cpp` grabs the window after each
batch, crops each cell and runs the same diff with the same thresholds. It needs
the Hermes host build and runs on CI.

The CMake step writes the document module and the cell layout:

```
node scripts/rnsvg-reference.mjs --corpus apps/rn-playground/rnsvg-corpus-generated --layout <build>/rnqnx-rnsvg-corpus-layout.json
```

`apps/rn-playground/rnsvg-corpus-generated/` is a build product and belongs in
`.gitignore`.

## Provenance

The fixtures are derived from react-native-svg 15.16.0 (MIT license, software-mansion),
`apps/common/example` and `apps/common/test`, tag `v15.16.0`. `Complex-1` is the
polygon bunny from `mikolalysenko/svg-3d-simplicial-complex` and `Complex-2` the
world map from `raphaellepuschitz/SVG-World-Map`, both as bundled in the
upstream examples. The image fixtures inline `example/assets/image.jpg` and
`test/assets/arrow.png`.
