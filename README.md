# Kazuya Training Lab — native v0.2 / cloud setup v0.2.2

**Experimental offline Training Mode source. Not a verified gameplay mod.**

The source from `Kazuya_Training_Native_v0.2_Source.zip` is now uploaded here so GitHub Actions can attempt compilation without installing compilers on the user's Windows PC. Build results are shown in Actions; committing source is not evidence of a successful build.

## Scope

Right D-pad on a GameCube controller requests a facing-relative forward / neutral / down / down-forward+A / release input sequence. The native adapter only permits it in offline Training Mode while the fighter is grounded and initially idle. Native game integration, controller-to-fighter mapping, and electric timing have not been validated on a Switch. Automatic DI follow-ups and Ferps-tech routes are **not implemented**.

The requested target is Smash Ultimate 13.0.5. The supplied configuration defaults to `enable_macro=false`. That mode observes inputs without injecting the sequence. The mapping assumes HID ID 0 corresponds to fighter entry 0; this assumption must be checked before enabling input changes.

## Cloud build

Open Actions → **Kazuya offline training - cloud build** → Run workflow on main. The job installs the host tools and Skyline compiler toolchain, runs the C input-engine and packaging tests, attempts `cargo skyline build --release`, and uploads logs. The workflow only produces the NRO artifact after successful native compilation and header checks. No deployment or changes to your Switch happen automatically.

The build has a 60-minute timeout. Dependencies are downloaded from their upstream repositories. The build records the resolved Cargo.lock in diagnostic artifacts; not all library revisions are pinned in this experimental source yet.

## First console test — only after a successful NRO artifact exists

1. Back up the existing SD-card Skyline plugins directory. Fully close Smash before editing the SD card. Do not overwrite or delete existing plugins.
2. Extract the successful `kazuya-training-lab-NRO-unverified` artifact. Copy the **contents** of its `SD_ROOT` folder to the root of the SD card. The new file belongs at `atmosphere/contents/01006A800016E000/romfs/skyline/plugins/libkazuya_training_lab.nro`. The configuration belongs at `ultimate/kazuya_lab/config.ini`. Source files, ZIPs, and ELF/object files are not installable plugins.
3. Leave `enable_macro=false`. Start offline Training Mode with Kazuya as human P1 using the GameCube controller. Release inputs, then press right D-pad once while idle.
4. Fully close Smash. Inspect `ultimate/kazuya_lab/runtime.log` for the detected version, `GC_HID_ID_0_SEEN`, and `DIAGNOSTIC_RIGHT_PRESS`. A logged D-pad press is only a wiring check, not proof of EWGF or hit confirmation.
5. If Smash crashes, remove only `libkazuya_training_lab.nro`, preserve the crash/runtime logs, and restore the prior setup. Compilation cannot guarantee compatibility with other installed plugins.

Do not enable the macro or treat a cloud build as hardware validation until the diagnostic mapping and stability have been checked. There is no online compatibility or ban-safety guarantee.

## Tests

`bash tools/test_core.sh` runs 38 synthetic input-engine cases, including 100,000 deterministic randomized invariant steps. `python3 tests/test_staging.py` checks packaging with artificial fixtures. Neither exercises the native Rust/Skyline hook or proves a perfect electric.

## Layout

- `src/`: experimental native bridge, game hook, and GameCube ABI declarations.
- `core/`: game-independent input-sequence implementation.
- `tests/`: synthetic host tests.
- `tools/`: build/test/package scripts.
- `config/`: diagnostics-only initial configuration.
- `.github/workflows/`: manual cloud build, logs, and output artifact.

No Nintendo game files, credentials, or private user files are included. Third-party dependencies retain their own licenses.
