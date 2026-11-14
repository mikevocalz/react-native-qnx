# First-time QNX developer study

This is a research plan, not a completed study. Recruit five to eight licensed
QNX developers who have not built this repository and did not write its docs.
Include keyboard-only use, a developer who uses enlarged text, and developers
familiar with each control layout. Record accommodation preferences without
collecting medical information. The facilitator must not be the document author.

The study asks whether a developer can identify the app, recover from a native
module mismatch, distinguish a Metro outage from a bundle error, and manage the
window using the downloaded runner. It also checks whether the launcher label
and both title-bar layouts communicate the same application identity.

## Preparation and consent

Use a release-candidate archive with verified hash and recorded provenance on a
named licensed image. Keep stock Pi 4 QSTI and custom Pi 400 sessions separate.
Record candidate, runtime and bundle hashes, board/image, input devices, display
scale, control layout, launcher font and viewport. Confirm normal input before
and after each session. Run an isolated non-premium sample through the singleton
guard. Preserve the owner's app, boot and window contracts.

Explain that the study tests the product and its instructions. Ask consent for
screen/audio capture, allow the participant to stop, and redact paths, machine
names and credentials before storing evidence. Do not request licensed SDP files.
Store raw observations privately; publish sanitized task results and timings.

## Interview guide

| Phase                        | Prompt and probes                                                                                                                                                             |
| ---------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Warm-up, 5 minutes           | What do you build on QNX? Which input devices and assistive settings do you usually use?                                                                                      |
| Context, 10 minutes          | Tell me about the last app you installed on a board. Where did you get stuck? How did you decide whether a failure came from the host, image or app?                          |
| Workflow, 15 minutes         | How do you usually add native dependencies? How do you know when a reload is sufficient? What would you expect the runner to preserve during a disconnect?                    |
| Surface reaction, 10 minutes | Show the five surfaces after task attempts. What does each state mean? What would happen if you used its primary action? Which part names the app and which names the runner? |
| Wrap-up, 5 minutes           | What would you try next without a facilitator? Which instruction would you change first? What did we miss?                                                                    |

Ask about behavior before asking for preferences. Avoid telling participants
that an action is safe or obvious. Ask them to think aloud, then remain quiet.

## Five usability tasks

| Task given to the participant                                                                                                  | Observe without coaching                                                                                                 | Completion evidence                                                                                                      |
| ------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------ |
| 1. Create the supplied app from the guide, change its name and icon through Expo config, and open it in the downloaded runner. | Can they find the launcher tile, connect identity across tile/title, and explain the connection state?                   | App runs; icon/name match; version/hash and command log saved.                                                           |
| 2. Use the supplied native-module fixture and make the app runnable again.                                                     | Does preflight name the module and lead to the custom-build action? Do they mistakenly keep retrying Metro?              | Incompatible stock runner refuses evaluation; compatible custom build accepts; participant describes why.                |
| 3. Recover after the facilitator disconnects Metro, then after the supplied bundle error.                                      | Can they distinguish retrying a connection from editing code and reloading? Are diagnostics readable and copyable?       | Reconnection and corrected bundle load; state survival and process ownership logged separately.                          |
| 4. Minimize, restore, move and close the test app using the keyboard; repeat with the other control layout.                    | Focus visibility/order, discoverability of restore, whether unavailable platform actions mislead.                        | One owned process/window; actions match target capabilities; cancellation and proceed cases recorded after guards exist. |
| 5. Find and open the short-name and maximum-fitting-name fixture tiles, then inspect the overflow fixture's preparation error. | Does a longer label alter tile size? Is the measured overflow actionable? Can the participant select a valid short name? | Equal tile geometry; overflow rejected before deployment; font/measurement and rendered captures saved.                  |

Task 2 depends on A; task 3 on A/B/G integration; task 4 on the windows API and
runtime thread split. Do not stage a mock success to close those tasks. The
downloaded-runner clean-host gate remains open until the real path exists.

## Recording and analysis

For each task record start/end, unassisted completion, completion after a prompt,
failure, wrong turns, requested help and exact quoted explanations. A facilitator
intervention ends the unassisted attempt. Define a ten-minute task limit before
sessions; report timed-out attempts rather than dropping them. Record exceptions
and recovery attempts. These are study settings, not measured product numbers.

Group observations by identity, compatibility, connection recovery and window
operation. Keep participant quotes separate from interpretation. Prioritize
failures that prevent running or closing the app, then recurring confusion,
then visual polish. Publish sample size, recruitment limitations and raw task
counts; do not claim statistical population estimates from a small study.
