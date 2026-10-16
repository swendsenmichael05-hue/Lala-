# Kazuya Training Lab v0.3.0

Experimental, offline Training Mode Skyline plugin for Smash 13.0.5.

D-pad Right retains the existing GameCube electric command and buffering.
An **opt-in** follow-up controller adds attributed-hit gating, sampled opponent
stick/velocity tracking, nair/electric loops, candidate finishers, platform
approach and tech-chase branches, and ordered custom route rules.

**This is not an all-roster or all-percent guaranteed zero-to-death mod.**
The combo module has synthetic tests, not console-calibrated route timings.
See [the full implementation and testing limits](docs/COMBO_LAB_0.3.0.md).

## Existing users

Replace only `libkazuya_training_lab.nro` in:

```
atmosphere/contents/01006A800016E000/romfs/skyline/plugins/
```

Preserve `ultimate/kazuya_lab/config.ini` with `enable_macro=true`.
New combos remain off unless you add `combo_assist=true`. The default command
mapping assumes A=Attack, B=Special, X=Jump. Restart Smash after changes.

Use offline 1v1 Training Mode, human P1 Kazuya and exactly one P2 fighter.
Manual attack/shield/jump cancels the follow-up controller. New target and route
telemetry goes to `ultimate/kazuya_lab/combo.log`; existing diagnostics continue
in `runtime.log`. Start with an idle electric and flat-ground first follow-up.

The full source-build artifact contains a diagnostics-only default config;
the user patch package deliberately omits it to avoid resetting existing config.

## Build and tests

```
bash tools/test_core.sh
python3 tests/test_staging.py
cargo skyline build --release
```

Use the provided GitHub Actions cloud build when the local Skyline toolchain is
not installed. Successful compilation alone does not establish in-game results.
Do not copy source files or host test executables to the SD card. Only use an
actual successfully produced NRO, back up the old one on the PC, and keep one
version in the active plugins folder.

No opponent state, damage, position, or cancel permission is modified. All
follow-up player actions use controller input reports. Unsupported states,
missing target data, timeouts and conflicting manual inputs cancel automation.

Historical patch notes are in `docs/`. `BUILD_RESULT.md` describes a prior build,
not a current v0.3.0 test certificate.
