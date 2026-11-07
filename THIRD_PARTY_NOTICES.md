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
| Qt                                                       | Source/module licenses and the applicable Qt agreement               | Resolve the QNX licensing gate before distribution; record every module and plugin                                                     |
| QNX                                                      | Licensed SDP/BSP/image and runtime terms                             | Obtain independently; do not include tools, sysroots, headers, vendor libraries, images or account/license material in public packages |
| Optional premium Forge packages                          | Separately licensed product terms                                    | Excluded from core platform distributions; follow [the consumer boundary](docs/policy/premium-components.md)                           |

Qt's [licensing documentation](https://doc.qt.io/qt-6/licensing.html) describes
commercial and open-source options and module-specific exceptions. Its
[QNX page](https://doc.qt.io/qt-6/qnx.html) names Device Creation Professional.
The private Pi 400 evaluation recorded in [ADR-0011](docs/adr/0011-qt-module-licensing.md)
expires on 17 November 2026 and does not establish a production distribution
basis. Preserve [the stop gate](docs/packaging.md).

If an LGPL route is accepted for an exact release, document the corresponding
source, notices, library replacement/relinking and installation obligations
that apply to that release. Public source availability or a successful rebuild
alone is not a licensing determination. Keep evaluation and licensed vendor
payloads out of npm tarballs, public artifacts and this repository.
