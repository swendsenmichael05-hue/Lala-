# Kazuya Training Lab v0.2.2: input-delivery timing patch

## Reported failure

The v0.2 console log showed the trigger being detected and phase 1 (forward) emitted, followed by CLOCK_OR_POLL_GAP with no neutral/down/attack output. The first example advanced the diagnostic callback epoch from 85 to 87 in 17 milliseconds. This does not mean the controller disconnected.

A synthetic regression with a forward output, two successive fighter observations, and then the next input poll reproduces phase=0/event=5 in the old C scheduler. That same regression produces phase=2/event=0 in the patched scheduler. This reproduces the scheduling pattern, not Smash's complete engine behavior.

## Change

Fighter observations now refresh identity, timestamp, and permission data without progressing the input sequence. A selected-controller poll can return at most one next phase after a new fighter observation. Repeated polls at the same observation epoch retain their phase; extra observations before a poll cannot consume an unreturned phase. A bounded 80ms per-output watchdog still cancels a real stall, and stale context, revoked training permission, disconnection, changed fighter identity, manual cancellation buttons, and backward clocks retain cancellation behavior.

The new runtime header reads `v0.2.2 TIMING PATCH`. C and Rust state layouts were updated together. The HID hook, controller slot, target game version, command values, and configuration path are otherwise unchanged.

## Tests

The local 38 general input-engine cases and 12 additional timing cases passed, along with five packaging cases. AddressSanitizer and UndefinedBehaviorSanitizer runs passed locally for both input suites. Timing regressions cover the 17 reported epoch-gap/delay pairs in both facing directions, batches of callbacks, repeated polls, true long stalls, and cancellation. These tests are synthetic and do not establish that a returned HID output was consumed in the intended simulation frame or recognized as EWGF.

Native compilation and its logs are recorded by the GitHub Actions run for commit 86a44bfd7488bf46fb47731241cb72791586290f. Compilation alone is not console validation.

## Updating an existing v0.2 installation

1. Fully close Smash and power off the Switch before removing its SD card.
2. Back up the existing libkazuya_training_lab.nro to the PC, outside the active Skyline plugins folder.
3. Replace only atmosphere/contents/01006A800016E000/romfs/skyline/plugins/libkazuya_training_lab.nro with the newly compiled plugin. Do not keep two renamed NRO copies in that folder.
4. Preserve ultimate/kazuya_lab/config.ini and its existing enable_macro=true setting. The full cloud artifact contains a default diagnostics-only configuration, so do not copy that default over the enabled configuration by mistake. The patch-only package omits configuration files.
5. Launch offline Training Mode at normal speed with Kazuya as human Player 1. Stand idle and release other inputs, then tap D-pad Right once. Test away from the opponent first so hitlag does not complicate this input-only check.
6. In the newest runtime.log section, check the v0.2.2 header and OUTPUT_FRAME phases 1, 2, 3, 4, 5. INPUT_SEQUENCE_FINISHED_NOT_HIT_CONFIRMED indicates output completion only. Confirm the actual move visually.

If it crashes, remove only this plugin and preserve the log. If it performs a different move or stops early, collect the newest runtime.log entries and the observed move. Do not change unrelated plugins, controller firmware, or adapter profiles to address this scheduler bug.

This patch still implements only the experimental offline EWGF input sequence. Automatic DI-routing, zero-to-death combos, and Ferps tech are not implemented.
