# Third-party notices and distribution boundary

The [MIT license](LICENSE) covers repository-authored code. Dependencies retain
their own copyright notices, licenses and distribution terms. This source
inventory is not a complete binary SBOM or permission to redistribute a target
runtime. A release must collect notices for its actual linked/plugin/font
closure, including transitive components.

| Component                                                | Source of terms                                                      | Packaging requirement                                                                                                                  |
| -------------------------------------------------------- | -------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| React and React Native                                   | Installed package `LICENSE` files (MIT)                              | Preserve upstream notices in distributions that contain them                                                                           |
| Hermes                                                   | Pinned Hermes source `LICENSE` (MIT), plus its external dependencies | Collect notices from the exact source/build closure                                                                                    |
| Nitro core, RN adapter and Nitrogen                      | Exact pinned packages and their license files                        | Preserve notices; generated/native dependencies need their own inventory                                                               |
| folly, fmt, glog, double-conversion, fast_float and Yoga | Each pinned source's license/notice files                            | Inventory the versions actually fetched or bundled by RN; root MIT does not replace their terms                                        |
| Qt                                                       | Selected open-source source/module licenses                          | Record the public-source build, every module/plugin, notices and applicable LGPL/GPL materials                                         |
| QNX                                                      | Licensed SDP/BSP/image and runtime terms                             | Obtain independently; do not include tools, sysroots, headers, vendor libraries, images or account/license material in public packages |
| Optional premium Forge packages                          | Separately licensed product terms                                    | Excluded from core platform distributions; follow [the consumer boundary](docs/policy/premium-components.md)                           |
| Source Sans 3 Regular and Light (host label measurement) | `native/app-identity/fonts/source-sans-3/LICENSE.md` (SIL OFL 1.1)   | Host-tool test data only, never linked or shipped in an app; keep the license file beside the fonts                                    |

Qt's [licensing documentation](https://doc.qt.io/qt-6/licensing.html) describes
commercial and open-source options and module-specific exceptions. Its
[QNX page](https://doc.qt.io/qt-6/qnx.html) names Device Creation Professional.
Open-source Qt is the selected path in [ADR-0013](docs/adr/0013-open-source-qt.md);
a paid subscription is not a project prerequisite. The private Pi 400 evaluation
recorded in [ADR-0011](docs/adr/0011-qt-module-licensing.md)
expires on 17 November 2026 and does not establish a production distribution
basis for those artifacts. Track the separate open-source build in
[packaging](docs/packaging.md).

For the selected LGPL route, document the corresponding
source, notices, library replacement/relinking and installation obligations
that apply to that release. Tie those materials to the exact source and build
closure. Keep evaluation and licensed vendor
payloads out of npm tarballs, public artifacts and this repository.
