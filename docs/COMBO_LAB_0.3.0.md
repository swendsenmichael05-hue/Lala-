# Kazuya Training Lab v0.3.0 — experimental combo controller

## Actual scope, not a completion claim

This version adds executable follow-up logic around the existing v0.2.7 electric
adapter. It is NOT a verified all-character, all-percent zero-to-death system.
No roster-specific timing or kill-percent table has been validated on a Switch.
The original electric input is unchanged and works independently with the combo
module disabled. All new behavior is opt-in, offline Training Mode, P1 Kazuya
against one fighter in P2. Another live fighter slot disables target selection.

## What the new code does

- Arms when D-pad Right starts an electric. It takes over only after observing
  an EWGF state, P1 collision-hit flag, a new increase in P2 damage, and P1 in
  the victim's last-attacker entry bitmask. Shield contact is excluded.
- Samples opponent character kind, percent, weight, mapped stick, position,
  velocity, damage/tech status, and supporting ground rays. The stick classifier
  is neutral/in/out relative to the opponent's side; it is NOT a read of the
  game's finalized DI calculation. Actual launch velocity is logged separately.
- Uses target velocity to aim drift and rechecks geometry between actions.
  Live data feeds a bounded state machine, not an unconditional timed recording.
- Attempts EWGF -> jump -> nair -> fast fall -> confirmed nair contact -> landing
  readiness -> another electric, with a default maximum of six EWGF hits.
- Includes up-smash, up-special, and an explicitly selectable Dragon Uppercut
  command-and-hold finisher. These are candidates, not confirmed kill windows.
- Includes an experimental Ferps-style sequence: forward setup, reverse jump,
  delayed double-jump neutral air using observed platform height, fast fall,
  and observed platform landing into tech chase. Timing has NOT been calibrated
  across stages, weights, launch angles, or percent. It may miss or abort.
- Branches on observed tech-in-place, tech rolls, and missed tech. Roll positions
  are tracked live; missed-tech handling attempts a crouch jab before electric.
  Its configured animation-end lead is a heuristic, not an invulnerability test.
- Supports up to 64 ordered custom rules matching fighter kind, percent range,
  sampled-stick class and elevated-platform presence. Rules select the existing
  route families. They are not an arbitrary script language or a trained model.

Default auto selects the platform candidate when elevated support is detected;
otherwise it selects nair below the configurable 95% threshold, and up-smash or
up-special above it. The 95% number is provisional tuning, NOT a kill-percent
claim. Weight is recorded for calibration, not used as a fake solved matchup
formula. Unknown characters use the generic candidates, not validated profiles.

## Installation and configuration

Close Smash and power off before removing the SD card. Back up the previous NRO
on the PC, outside the active plugins directory. Replace only:

    atmosphere/contents/01006A800016E000/romfs/skyline/plugins/libkazuya_training_lab.nro

Keep the existing `ultimate/kazuya_lab/config.ini`. The patch ZIP intentionally
contains no live config.ini. Add this line to enable follow-up control:

    combo_assist=true

Keep `enable_macro=true`. Existing fast_electric, fast_movement, fast_recovery
and right_diagonal_priority options are unchanged. Restart Smash to reload.

First-use input assumptions: physical A maps to Attack, B to Special, X to Jump.
`combo_jump_bit=2` selects HID X; `combo_jump_bit=3` selects HID Y. No adapter
remapping is performed. A nonstandard button layout requires matching settings.

For a read-only target/hit diagnostic before allowing follow-ups, use:

    combo_assist=false
    combo_observe=true

The original electric still injects its command, but observer mode never adds
combo inputs. It records hit/stick/launch observations after the starter.

## Controls and cancellation

Tap Right: attempt electric, then one bounded follow-up route if the hit gate
passes. A whiff or shield contact does not start a route. Releasing Right does
not cancel an already accepted combo. Press a manual attack, shield, or jump
button to abort; manual inputs take priority. The maximum active route duration
is ten seconds, with additional phase, delivery-gap and last-hit timeouts.

Holding Right does not let the old electric-repeat engine fight the combo
controller. After a combo ends while Right is still held, release and press
again to rearm. Outside an active combo the earlier hold-repeat behavior is
preserved. Getting hit, target loss/replacement, reset, disconnect, unsupported
geometry, stale observations or leaving Training Mode cancels the route.

## Optional tuning, all read at startup

    combo_route=auto
    combo_max_links=6
    combo_ferps=true
    combo_finisher_percent=95
    combo_jump_bit=2
    combo_nair_x=17
    combo_nair_y=14
    combo_ferps_below=10
    combo_tech_lead=11
    combo_drift_gain=0.14

Route names: auto, nair, usmash, upb, ferps, dragon. Forced Ferps still requires
an elevated support ray. Numeric values are bounded; they require console
calibration. The original successful electric pulse is not shortened further.

Custom CSV path: `ultimate/kazuya_lab/combo_rules.csv`. There is no header row.
Format: fighter_kind,min_percent,max_percent,di,platform,route.
The `-1` wildcard is allowed for fighter/di/platform. DI is 0 neutral, 1 in,
2 out. Platform is 0 flat, 1 elevated. Rows are first-match wins, maximum 64.
The link cap takes priority so a custom nair rule cannot make an infinite loop.
The bundled EXAMPLE file is deliberately inactive and contains no verified
character prescriptions. Kind numbers are logged from the running game.

## Test sequence and logs

Start offline Training Mode at normal speed with one P2 fighter, no items or
hazard interference. Check the original idle electric first. A whiff must not
make Kazuya jump. Hit a stationary P2 on flat ground; observe the first follow-up.
Then test neutral/in/out DI settings, a low platform, and tech options separately.
Do not infer success from a sequence-completed message: inspect what move hit.

`runtime.log` starts `v0.3.0 COMBO LAB`. New telemetry is in
`ultimate/kazuya_lab/combo.log`. Useful events: COMBO_ARMED,
ATTRIBUTED_EWGF_HIT, TARGET_STICK_CLASS_CHANGED, TARGET_LAUNCH_MOTION_OBSERVED,
COMBO_ROUTE_SELECTED, ATTRIBUTED_NAIR_HIT, PLATFORM_SEQUENCE,
TECH_OPTION_OBSERVED, COMBO_CANCELLED, COMBO_TIMEOUT, COMBO_TARGET_LOST.
The logger's route/phase describe the current state when drained; do not treat
all phase events as a full frame-by-frame replay. No private opponent data are
sent anywhere; game telemetry stays on the SD card.

## Validation boundary

63 synthetic C cases exercise hit gating, routes, both facings, custom rules,
manual overrides, stale data, platform actions, timeouts and 100000 randomized
safety checks. The suite runs optimized and with AddressSanitizer/UBSan; an
AArch64 freestanding object is also checked. Existing electric/request/timing/
nair/regression and staging tests remain. These tests are NOT game simulation,
console compatibility, DI correctness, guaranteed hitstun, or KO verification.
Native compilation and the resulting binary must be checked separately.

## Sources and unresolved evidence

Primary input/mechanics references inspected for this implementation:
- Hit Box Arcade, SSBU EWGF inputs (neutral is optional):
  https://www.hitboxarcade.com/blogs/smash-box/ssbu-ewgf-on-smash-box
- Ultimate Frame Data, Kazuya inputs and attack data:
  https://ultimateframedata.com/kazuya
- water smash's original 2022 Ferps-tech timing investigation:
  https://note.com/water_smash/n/n93aa58f324fa
  It reports stage/character/DI/percent dependencies; not a universal kill route.

Native API references were checked against upstream source:
- https://github.com/jugeeya/UltimateTrainingModpack/blob/main/src/common/mod.rs
- https://github.com/HDR-Development/HewDraw-Remix/blob/pre-release/utils/src/modules/meter.rs
- https://github.com/HDR-Development/HewDraw-Remix/blob/pre-release/fighters/reflet/src/status/float.rs

Discovery-only leads, NOT transcribed or claimed as imported route data:
- Riddles video: https://www.youtube.com/watch?v=UMSXxFZNps0
- Ferps post: https://x.com/ferpsfgc/status/1493667287911518208
- Vied Viper post: https://x.com/ViedViper/status/1711784816289403059
- https://kazuyamishima.com/ (JavaScript-dependent database; routes not extracted)

Those video/post contents were not accessible in this session. No claim is made
that every Twitter/YouTube guide or every character's custom routes was added.
