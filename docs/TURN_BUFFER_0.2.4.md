# Kazuya Training Lab v0.2.4 - turn and buffer patch

## Why this update exists

The user's v0.2.3 log contains completed command sequences and buffered follow-ups,
but also movement requests expiring after roughly 400ms and two cancellations
when dash changes into status 484. Skyline-smash names 484 (0x1E4)
FIGHTER_DEMON_STATUS_KIND_TURN_AUTO. That state was absent from our movement
allowlist. Some neutral-braking output reports also retain stick-direction
pseudo-buttons although all four analog coordinates are zero.

These observations identify code defects/limitations. They do not show a broken
controller, a missing installation, or a need to change unrelated mods.

## Changes

- One queued tap now lasts at most 1000ms, previously 400ms. This accommodates the
  observed movement-stop waits and early presses during electric recovery.
- Kazuya TURN_AUTO, WALK_BACK, WALK_BRAKE_BACK and TURN_RUN_BACK join DASH_BACK
  and the existing generic movement states as queueable movement. These states
  DO NOT become direct electric-start states; native standing WAIT is still
  required. Current facing is captured when the command begins.
- While the plugin owns neutral movement-braking axes, it also clears the
  analog direction pseudo-buttons (HID bits 16 through 23). Manual cancellation,
  other controller slots, disabled diagnostics and out-of-scope input keep
  their original physical reports.
- The v0.2.2 five-step electric pulse and 80ms phase watchdog are unchanged.
- Hold-to-repeat remains recovery-gated. Attack, shield, jump and other manual
  cancellation buttons still clear pending requests. Unsupported attacks,
  shield states, landing states and airborne use have not been added.

IMPORTANT: Releasing a separately queued tap does not undo that tap. It may
execute up to ONE SECOND later when WAIT is reached. Manual attack/shield/jump
cancels pending work. Releasing a continuous hold stops future hold repeats;
an already running five-step input sequence is allowed to finish.

This is not an instant dash cancel, recovery bypass, out-of-shield electric,
DI-aware combo router, Ferps-tech macro or online mod.

## Validation

The unchanged pulse/timing suites are retained. The request suite adds tests
for longer dash/auto-turn observations, early recovery taps, exact 1000/1001ms
expiration, all eight stick-direction bits, the reported 0x64000 input,
manual-cancel report preservation and out-of-scope pass-through. Expiration
tests were updated to the new documented 1000ms limit, not removed.

GitHub Actions is the native compile/test authority. A successful build or
synthetic test is not verification of v0.2.4 on a Switch. The previous idle
pulse was user-tested; new turning and buffer behavior requires a new test.

## Existing installation

1. Fully close Smash, then power off before removing the SD card.
2. Back up the old libkazuya_training_lab.nro outside the active plugins folder.
3. Replace ONLY atmosphere/contents/01006A800016E000/romfs/skyline/plugins/libkazuya_training_lab.nro.
4. Preserve ultimate/kazuya_lab/config.ini and enable_macro=true. A patch-only
   ZIP has no config.ini and cannot reset this setting.
5. Fully restart Smash. The newest runtime.log section must begin with
   v0.2.4 TURN + BUFFER PATCH.
6. Test offline Training Mode, normal speed, human P1 Kazuya: idle tap; dash then
   tap while stopping; turnaround then tap; hold for several electrics and
   release; one tap early during recovery to queue one more.
7. Keep testing away from the opponent first. Check actual on-screen behavior,
   not only INPUT_SEQUENCE_FINISHED_NOT_HIT_CONFIRMED. That line is output
   completion, not confirmation of a hit or an unavoidable combo.

Remove only this plugin if the game crashes and preserve the runtime log.

## Primary identifiers checked

https://raw.githubusercontent.com/ultimate-research/skyline-smash/master/src/lua_const.rs
https://raw.githubusercontent.com/switchbrew/libnx/master/nx/include/switch/services/hid.h
