# QNX v1 surface references

Reviewed on 2026-11-14 for workstream G. These are visual references for the
five surfaces, with three inspected Mobbin previews per surface. The title-bar
searches returned web application headers and browser mockups; they do not
establish native macOS, Windows or QNX behavior. ADR-0016 and the actual QPA
capability result govern window behavior. No assets were copied or redistributed.

| Surface              | Reference                                                                                     | Observed pattern to borrow                                                                                                         |
| -------------------- | --------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| G1 macOS title bar   | [GoDaddy editor](https://mobbin.com/screens/e9635a15-7522-4abb-b2a8-3cd1fe713239)             | Separate the app's fixed top chrome from editable content; the preview has a small browser-style header.                           |
| G1 macOS title bar   | [Notion preview](https://mobbin.com/screens/42f72d81-0036-4b07-87ed-9ac157f0ac4f)             | Reserve a narrow title strip above document content and keep actions outside the title's reading area.                             |
| G1 macOS title bar   | [Fibery document](https://mobbin.com/screens/ea13b251-aac6-4966-a557-04c2efc37aa3)            | Keep the document name at the top and secondary actions at the edge, leaving content dominant.                                     |
| G2 Windows title bar | [Chronicle editor](https://mobbin.com/screens/931b4a2c-28d9-4cbc-843c-a9312c5329e6)           | Align document identity at the leading side and group actions at the trailing edge.                                                |
| G2 Windows title bar | [HoneyBook document](https://mobbin.com/screens/84d97ae3-34f5-4f73-9bab-fbff6c391dc4)         | Keep a compact identity group and an isolated action group with visible boundaries.                                                |
| G2 Windows title bar | [Ferndesk preview](https://mobbin.com/screens/b4152816-5ade-4526-b8ae-53251d01ecbb)           | Preserve a thin preview header rather than adding a second product toolbar. The pictured traffic lights are not Windows controls.  |
| G3 launcher tile     | [Adobe app grid](https://mobbin.com/screens/62de365c-f2b9-46a9-a490-8b5b0ad6b010)             | Repeated cells keep the icon/name relationship and row rhythm stable across app names.                                             |
| G3 launcher tile     | [Okta app grid](https://mobbin.com/screens/071122e8-7d2b-4267-bfaa-5a216d0ac8d5)              | Equal card sizes and centered icon/name groups make application scanning predictable.                                              |
| G3 launcher tile     | [Basecamp report grid](https://mobbin.com/screens/b0215a9a-02a9-4a9d-bd45-110c97fe295e)       | Place a recognizable symbol above a concise label and keep every grid cell equal.                                                  |
| G4 preflight         | [v0 runtime error](https://mobbin.com/screens/a745917c-fd1e-4499-be25-c4d8063802b9)           | Separate the technical diagnostic from the main content; expose details without making them the headline.                          |
| G4 preflight         | [Google AI Studio failure](https://mobbin.com/screens/bd6669d2-d3df-476e-b933-e44bb25e5259)   | Place the failure reason next to a specific recovery action. Replace its generic heading with the named runner problem.            |
| G4 preflight         | [Unity package restrictions](https://mobbin.com/screens/b7517dcf-f087-4b39-a6c3-be1d837a6702) | Use rows to name each package and its status so multiple incompatibilities can be scanned.                                         |
| G5 dev states        | [Google Meet recovery](https://mobbin.com/screens/e5069b49-767a-4c2f-a3f2-6676a13f7b54)       | Keep recovery actions together beneath a short cause/instruction block. Use a specific Metro heading instead of its generic error. |
| G5 dev states        | [Zoom connection timeout](https://mobbin.com/screens/d49906e1-327c-4fa7-8159-cdbb1a573ce7)    | Keep the application behind the failure panel and distinguish retry from leaving.                                                  |
| G5 dev states        | [Steep connection failure](https://mobbin.com/screens/c70f80de-5c92-4044-b3b5-b44b819647b9)   | Show the failed connection's context, a bounded technical detail block and one retry action together.                              |

## Source boundaries

Local inspection covers `native/window-chrome/WindowChrome.qml`, its controller,
model and Quick tests; `native/app-identity/LabelFit.cpp` and its header;
`packages/react-native-qnx-cli/src/desktop-entry.mjs`; the inline overlay in
`vnext/src/runtime/QnxDevSupport.cpp`; ADR-0016; and the existing app handoff.
The implementation base is `f87c742`. Newer open PR device evidence must be
reviewed separately before changing an implemented-status statement.

The [QNX simple-terminal README](https://gitlab.com/qnx/projects/simple-terminal/-/blob/main/README.md)
was read. It establishes QNX build/deployment prerequisites; its Screen/input
source was not inspected in this slice. H20 remains open for that inspection.
No convention is inferred from the README alone. The
[Qt target requirements](https://doc.qt.io/qt-6/qnx-target-requirements.html)
were opened as a platform reference; custom cursor behavior still needs the
pinned QPA source and per-target receipt.

The local skill bodies were read in the requested design sequence. Their
versions, byte lengths and SHA-256 values are in [skills.json](skills.json).
The files came from the installed plugin cache; the hashes identify those
bytes and do not establish upstream authenticity. This slice writes design
documents only. It does not claim a Figma artifact, user interviews, UI code,
device execution, an accessibility conformance result or completed gate.

---

**Your Mobbin AI usage is high this month**

Usage is unlimited during beta. If your usage stays at this level, you may need to purchase more credits once limits take effect. [View usage and plan options](https://mobbin.com/settings/billing-and-usage?utm_source=chatgpt&utm_medium=ai-assistant&utm_source_platform=mcp)
