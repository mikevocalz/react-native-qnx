# Roster

The names below are review lenses. A lens is a question to ask of a page. It does not mean the person reviewed, endorsed or knows about this documentation.

## Engineering lenses

| Lens | Question to ask of a page |
| --- | --- |
| Marc Rousavy (Nitro) | Does the native-module page match the pinned Nitro alpha, and does it state what has run? |
| Nick Gerleman (Fabric, Yoga) | Does the page say Yoga owns layout and Qt items are absolute leaves? |
| Joshua Gross, Samuel Susla (Fabric) | Is the Fabric mounting and scheduling description accurate to the installed source? |
| Kudo Chien (Expo) | Do the Expo setup commands match what `project create` and `project add-qnx` print? |
| Jamie Birch (expo-desktop) | Would a desktop-target developer find the window and identity settings where they expect them? |
| Jay Meistrich (Legend Spark) | Is the first run as short as it can honestly be? |
| Krzysztof Magiera (Reanimated, Gesture Handler) | Are the proven gestures named, and the unproven ones named as unproven? |
| Lars Knoll, Ulf Hermann, Shawn Rutledge (Qt) | Are Qt module tiers, licences and the Qt Quick scene graph described correctly? |
| Rob Hogan, Alex Hunt (Metro) | Do the bundle and dev-server pages match Metro 0.87.1 behavior? |

## Documentation lenses

| Lens | Question |
| --- | --- |
| Daniele Procida (Diataxis) | Does each page do one of tutorial, how-to, reference or explanation? |
| Fuma Nama (Fumadocs) | Does the site use Fumadocs conventions: page tree, search, llms.txt? |
| Sebastien Lorber (Docusaurus) | Are versioning and link integrity handled? |

## Design lenses

Rasmus Andersson, Emil Kowalski, Paco Coursey: type scale, motion, and density. Basement Studio, Instrument, Pentagram: layout and hierarchy. Margelo, Software Mansion, Callstack, Infinite Red: how a React Native library presents itself to developers.

## Roles in this pass

| Role | Task | Output |
| --- | --- | --- |
| A. Audit | Read the repository and list what exists against what docs claim | Notes used to choose pages |
| B. Parity | Compare against Spark and expo-desktop docs and list gaps | Not run in this pass |
| C. Verifier | Run every command shown in a page, from a fresh clone | The [verification log](../content/docs/reference/verification-log.mdx) |
| D. Writer | Draft pages from A and C only | The pages in `content/docs` |
| E. Reviewer | Follow the quick start cold, with no context, and file defects | Recorded in the pull request |

Roles A and B were not run as separate agents in this pass. The writer read the repository directly, so page content is limited to what the repository and the verifier's log support.
