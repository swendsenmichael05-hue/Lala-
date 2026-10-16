# Kazuya Training Lab v0.2.6 - nair buffer and recovery gate

## Diagnosis from the supplied v0.2.5 trace

The current trace contains 36 command starts, 33 EWGF-state observations and
13 REQUEST_BLOCKED_STATE events with status 24 / motion 0xd562c06c5.
24 (0x18) is LANDING_ATTACK_AIR; hash40("landing_air_n") is 0xd562c06c5.
The compact command reaches its attack output in 28-34ms in this trace;
the 33 EWGF observations occur 36-43ms after software sequence start.
These timings exclude physical button-to-hook latency, queue waiting, move
startup to the active hitbox and display delay. They are not press-to-hit times.

Four complete movement waits were 434, 434, 333 and 317ms from queued request
to command start. The recovery flag was still waiting for WAIT after a prior
electric even though the fighter had already moved into a locomotion state.
Three other attempts entered different statuses (498,501,500), not the electric.
This patch does not relabel those as successes or claim all movement inputs work.

## Changes

- Nair in the air, its normal fall continuation and its landing can retain a
  single pending request, still bounded to 1000ms from the latest accepted tap.
- No electric inputs are emitted while airborne. The plugin does not change
  air drift, fast fall, landing lag or animation speed to make nair end earlier.
- On landing, it waits for the existing native CancelModule permission or
  standing WAIT before starting the unchanged compact input sequence. It only
  reads cancel permission; it does not enable it or force a game transition.
- A real observed EWGF status followed by supported non-turning movement can
  release the stale recovery latch when fast_movement=true. The initial forward
  pulse entering DASH cannot do this: real EWGF observation is required first.
- Extra logs distinguish nair buffering, native landing permission and recovery
  exiting to movement. request_wait_ms on INPUT_SEQUENCE_STARTED is the wait
  from its queued tap, or -1 for a repeat without a tracked fresh request.
- Input engines, compact/legacy timing, controller slot and config path remain
  unchanged. Ordinary shield actions, other aerials and unrelated attacks have
  not been added as supported macro-start states.

Manual attack/shield/jump inputs still cancel pending requests. After initiating
nair, release A/jump before tapping Right. A released queued tap may execute up
to one second later. Manual cancellation removes it. Holding Right retains the
existing repeat behavior after the first attempt; releasing stops future hold
repeats, not an already executing command.

This is an experimental offline Training Mode plugin. It does not contain DI
routes, Ferps tech or guaranteed zero-to-death follow-ups. It does not establish
the earliest possible legal input from every movement/recovery state.

## Test status

26 new dependency-free Rust policy tests and 16 C request integration cases are
added to cloud CI. The C cases run optimized and with ASan/UBSan. Existing core,
timing, request, speed and staging tests are retained. Refer to the actual cloud
run and its diagnostics for pass/fail; compilation is not Switch validation.

## Updating the current installation

Close Smash and power off the Switch before removing the SD card. Back up the
old NRO onto the PC outside the active plugins folder. Replace only:

atmosphere/contents/01006A800016E000/romfs/skyline/plugins/libkazuya_training_lab.nro

Keep config.ini and enable_macro=true. No new setting is required. Patch-only
ZIPs omit the config so enabled settings cannot be overwritten by defaults.
Keep only one plugin copy in the active Skyline plugins folder. Fully restart.

Expected header: v0.2.6 NAIR BUFFER + RECOVERY PATCH

Offline Training Mode, normal speed, human P1 Kazuya: confirm idle electrics,
then nair -> release A/jump -> tap Right before landing, and separately tap Right
during nair landing recovery. Keep your usual drift/fast-fall movement. Finally
check electric -> dash -> Right. Test both facing directions away from the CPU
first. Send the latest runtime.log and describe the actual move on screen.

REQUEST_BUFFERED with nair_buffer=true means the request was held.
NAIR_LANDING_NATIVE_CANCEL_READY means native landing-cancel permission was seen.
EWGF_RECOVERY_EXIT_TO_MOVEMENT means the confirmed electric has exited to a
supported direct-movement state; it is not forced early recovery.
EWGF_STATE_OBSERVED_NOT_HIT_CONFIRMED means recognition, not a connected hit.

If a movement attempt produces another move, fast_movement=false remains the
conservative fallback (restart required). Compact idle timing remains unchanged.
Remove only this plugin if the game crashes and preserve logs.

## Primary references

https://raw.githubusercontent.com/ultimate-research/skyline-smash/master/src/lua_const.rs
https://raw.githubusercontent.com/ultimate-research/skyline-smash/master/src/cpp.rs
