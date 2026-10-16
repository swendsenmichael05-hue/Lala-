# v0.2.3 - dash request buffer and hold-to-repeat

This adds a request gate around the unchanged v0.2.2 EWGF pulse engine.
The user reported that v0.2.2 produced electrics from idle but taunted after
some dashes and did not repeat. The old trigger only accepted a rising edge
while in WAIT, and deliberately prevented held-button retriggers.

## Behavior when enable_macro=true, offline Training Mode only

- Tap Right: one attempt, or one pending request for up to 400ms.
- Tap while moving: suppress the taunt and neutralize the sticks while
  waiting for the game's normal standing WAIT state. No forced status,
  animation cancel, recovery skip, or claim of earliest possible dash cancel.
- Tap during electric recovery: retain one request, expiring after 400ms.
  More taps refresh that single request, not a multi-attack backlog.
- Hold Right: repeat after the current command completes AND the fighter
  has been observed busy and then returned naturally to WAIT.
- Release Right: stops future hold repeats. An already executing command
  finishes; a separately tapped pending request can still run within 400ms.
- Manual attack/shield/jump buttons cancel queued/held input. Leaving the
  supported grounded training context or losing controller/context cancels.
- The existing 80ms per-output watchdog stays intact; a 2500ms recovery
  watchdog cancels a held request that never becomes actionable.
- Right is consumed in the supported grounded training context even when
  rejected. Diagnostics mode and inputs outside that context are unchanged.

Supported movement states include walk, dash, run, turn and run braking.
Only WAIT starts the electric. Only the specified crouch-dash/WGF/EWGF states
are queued as attack recovery; this is not a universal attack-cancel system.
Native status transitions, not guessed recovery frame counts, gate repeats.

## Validation

The new request tests cover left/right output preservation, dash neutral
buffering, expiration, repeated taps, held repeats, release, manual overrides,
wrong controllers, game-context changes, hitlag handling, timeouts, diagnostics,
repeated polls, and 100000 randomized safety checks. Both optimized and
AddressSanitizer/UndefinedBehaviorSanitizer runs are used. The existing core
and timing suites are retained in cloud CI, not replaced by the new tests.
Synthetic tests and a successful native compile are not Switch verification.

## Existing installation

Replace only libkazuya_training_lab.nro. Keep your enable_macro=true config.
Store any old NRO outside the active plugins folder; never load both copies.
The patch-only ZIP excludes config.ini so it cannot reset your setting.

Fully restart Smash, choose offline Training Mode, human P1 Kazuya, normal
speed, and test away from the opponent first:
1. Tap Right while standing idle.
2. Dash, release the stick, tap Right during braking. It may wait briefly.
3. Hold Right for several attacks, then release it.
4. Tap Right during the end of an electric's recovery to queue one more.

Latest runtime.log header: v0.2.3 BUFFER + REPEAT PATCH.
REQUEST_BUFFERED means a request was held, not that an electric was emitted.
REQUEST_EXPIRED is a tap that waited over 400ms, not a crash.
OUTPUT_FRAME phase 0 is neutral movement braking; phases 1-5 are the existing
command inputs. INPUT_SEQUENCE_FINISHED_NOT_HIT_CONFIRMED still proves only
output completion, not hit confirmation. Record the actual move seen on screen.

The plugin is still electric-only and offline-only. DI-aware combo followups,
zero-to-death routes, and Ferps tech are not implemented.
