# Kazuya Training Lab v0.2.5 - faster command input

## What changed

The optional neutral step can be omitted in Smash's electric input. This is
explicitly documented by Hit Box Arcade in its SSBU EWGF guide:
https://www.hitboxarcade.com/blogs/smash-box/ssbu-ewgf-on-smash-box

The compact mode now emits F, D, DF+A, release rather than F, neutral, D,
DF+A, release. No new A hold is added. These are separate observed input
intervals; the code does not squeeze all steps into a single poll. Phase
labels remain backward-compatible: 1,3,4,5 (phase 2 is intentionally absent).

In the provided v0.2.3 console log, first output to attack output was about
50ms. Removing one input interval is intended to reduce that to about 33ms
at the same ~60Hz cadence. This is an expected command-input saving, NOT a
measured v0.2.5 console improvement or total press-to-hitbox latency. The
move's own startup, recovery, display latency and physical input polling
are separate. This does not establish the globally fastest possible macro.

The experimental fast_movement path starts an initial request without first
waiting for native WAIT during supported non-turning walk, dash, run, run
braking, backdash, backward walk and backward braking states. It sends the
normal input command; it does not enable transition terms, change game
statuses, accelerate animations or bypass hitstun. Recognition from these
states must be tested on the actual console. Turn/auto-turn states still
buffer. Unsupported shield, attack, airborne and landing cases are not added.

Repeated attacks continue to require the normal observed recovery back to
WAIT. The movement shortcut cannot unlock a pending recovery gate. The
single-tap buffer remains 1000ms, not a forced one-second delay. Manual
attack/shield/jump cancels pending requests. Release stops future hold
repeats, but an already executing command finishes and a separately queued
tap can still execute within its window.

## Installation

Close Smash and power off before removing the SD card. Back up the old NRO
on the PC, outside the active plugins folder. Replace only:

atmosphere/contents/01006A800016E000/romfs/skyline/plugins/libkazuya_training_lab.nro

Keep ultimate/kazuya_lab/config.ini with enable_macro=true. The two new
settings default true when absent, so no configuration editing is needed:

fast_electric=true
fast_movement=true

The patch-only download omits config.ini. Do not overwrite an enabled config
with the diagnostic default from a full build artifact. Only one version of
the plugin should be present in the active plugins directory. Fully restart.

For a fallback without swapping NROs, add fast_electric=false to restore the
old neutral-step command, or fast_movement=false to restore the old full-stop
start rule. Set both false for the v0.2.4 input behavior. Settings are read at
Smash startup. Missing enable_macro still disables automation.

## Testing and limits

Offline Training Mode, normal speed, human P1 Kazuya using the verified HID
slot. First test away from the opponent: idle right-facing and left-facing
taps, then an initial dash tap and backdash tap. Next test held repetition
and release. Inspect the move on screen, not just input-completion messages.
If a movement attempt does not produce an electric, do not assume a quicker
input was a successful input; send the newest log and report the actual move.

Expected log header: v0.2.5 FAST INPUT PATCH.
FAST_ELECTRIC and FAST_MOVEMENT are printed at startup.
OUTPUT_FRAME includes since_start_ms measured from the software sequence start.
EWGF_STATE_OBSERVED_NOT_HIT_CONFIRMED logs observation of the game's electric
status after an attack pulse, with elapsed time; it is not a hit confirmation
and not the time the physical D-pad switch closed. No damage or target data
are being checked. Absent markers can also reflect cancellation or logging.

25 new synthetic speed tests include legacy/compact comparisons, both facings,
manual overrides, disabled/out-of-scope input, movement gating, retained
recovery rules, watchdogs, and 100000 randomized checks. Optimized and
AddressSanitizer/UndefinedBehaviorSanitizer versions are run. Existing core,
timing, request and packaging suites remain in CI. A successful compile and
synthetic tests do not prove console timing or compatibility.

Still electric-only and offline-only. DI-routing, whole-roster combos and
Ferps-tech automation have not been implemented in this plugin.
