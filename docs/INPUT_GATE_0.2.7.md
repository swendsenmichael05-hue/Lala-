# Kazuya Training Lab v0.2.7 - input gate patch

## Evidence from the uploaded v0.2.6 session

The latest section has 65 input starts and 53 EWGF-state observations, not a
100% success rate. Completion messages alone are not EWGF or hit confirmation.
Four airborne nair requests are cancelled at touchdown. The old policy rejects
mixed snapshots such as grounded ATTACK_AIR/attack_air_n, before landing state
and motion agree. This is a reproducible policy gap consistent with the trace;
the log does not expose every intermediate callback, so the precise runtime
ordering remains an inference to validate on console.

Four started landing attempts reach status 492 instead of electric status 493.
Other attempts reach non-electric standing attack states. These recognition
failures are not established as solved by this patch. Input-history and game
parser timing need console validation. The working C compact pulse is unchanged.

The log also repeatedly records buttons=0xc000, which is D-pad Right plus Down
(HID bits 14 and 15), not the analog fast-fall direction. The old cancellation
mask treats Down as a competing manual button. Finally, electric recovery often
has native_cancel=true while the old gate continues to wait for standing WAIT.

## Changes

- Preserve identified nair requests across grounded/airborne status-boundary
  snapshots and known old/new nair motion at touchdown. Transitional observations
  remain queue-only: no airborne input injection or skipped landing lag.
- Right with D-pad Up/Down gives Right priority in the enabled, connected, fresh
  offline context. Standalone Up/Down, Left, manual A/shield/jump and other pads
  keep their existing behavior. Raw combined buttons remain visible in event logs.
- EWGF/WGF recovery can start the next command after the game's own existing
  CancelModule permission becomes true; standing WAIT is no longer mandatory
  for that narrow case. No cancel permission, status, animation or damage data
  is written. It is not a universal out-of-attack or out-of-shield macro.
- Track a new electric entry instead of letting the previous electric's recovery
  falsely confirm a repeat. Log NON_ELECTRIC_ATTACK_OBSERVED on recognized wrong
  outcomes, ELECTRIC_NATIVE_CANCEL_READY for recovery readiness, and
  DIAGONAL_TRIGGER_NORMALIZED for combined-direction handling.
- queued_age_ms reports the live queue age. ready includes the direct movement
  path; recovering is logged separately. request_wait_ms on sequence start is
  still measured from the tracked request, not physical button-switch latency.

## Controls and settings

Existing enable_macro=true configuration is preserved. New options default true:

fast_recovery=true
right_diagonal_priority=true

Set either false and restart Smash to fall back for that individual change.
fast_electric and fast_movement keep their prior settings. A missing enable_macro
still disables automation. Release A/jump before requesting the electric.
A tapped request may execute within one second even after Right is released.
Releasing a hold stops future repeats, not an input sequence already in progress.
Manual competing gameplay buttons cancel pending requests.

## Validation

Keep all existing pulse, timing, request, speed, nair and staging tests.
A new host integration suite links the Rust policy/FFI directly against the C
request and pulse objects. Seventeen added tests cover touchdown ordering,
recovery permission, hitlag, manual overrides, diagonal normalization and stale
EWGF confirmation. The legacy policy tests are also loaded by that suite.
Successful tests/build are not Switch verification, perfect-hit proof or a
claim of the fastest possible legal input in every state.

## Install and test

Fully close Smash and power off before removing the SD card. Back up the old NRO
on your PC, outside the active plugins folder. Replace only:

atmosphere/contents/01006A800016E000/romfs/skyline/plugins/libkazuya_training_lab.nro

Keep ultimate/kazuya_lab/config.ini with enable_macro=true. A patch-only ZIP omits
config.ini. Fully restart Smash; the latest log should start:

v0.2.7 INPUT GATE PATCH

Offline Training Mode, human P1 Kazuya, normal speed: check idle electric first;
nair then release A/jump and tap Right once before touchdown (do not retap after
landing); nair landing with Right plus Down; hold Right for several electrics and
release. Test without an opponent first, then report actual moves and the newest
runtime.log section. Remove only this NRO if the game crashes.

DI follow-ups, roster-specific routes, Ferps tech and online use are not added.
