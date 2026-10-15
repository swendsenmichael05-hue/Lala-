# Native cloud build result — 2026-10-15

**Compilation succeeded. Switch runtime has NOT been tested.**

- Workflow run: https://github.com/swendsenmichael05-hue/Lala-/actions/runs/38815648142
- Run number: 2; conclusion: success.
- Built source commit: `96351751b66068a0ddd2dd0733f5f1f99225510a`.
- Native package version: 0.2.0; cloud setup revision: 0.2.2.
- Compiler: Skyline custom Rust 1.95.0-nightly (2026-02-14 toolchain), cargo-skyline 3.5.0.
- Native compile completed in release mode. The initial compile error was fixed by using the hook-local `original!()` macro rather than `skyline::original!()`.
- Host validation: 38 input-engine tests, including 100,000 randomized invariant steps; 5 packaging tests. These are not console tests.
- Artifact: `kazuya-training-lab-NRO-unverified`, artifact ID `12010408433`.
- Plugin: `libkazuya_training_lab.nro`, 118784 bytes.
- NRO SHA-256: `6f6e92b9a8df140b5892b0719ea9c458754190ce1483d67c5bd6bef81897bc97`.
- Downloaded artifact ZIP SHA-256: `e01bbd74da23b925c0ea94022e48c78d1fad6e182ea29c111be44c2e2cbf6b15`.

The downloaded ZIP CRC, GitHub artifact digest, NRO recorded checksum, NRO0 header, declared image size, segment bounds, and `enable_macro=false` configuration were verified. The binary was not executed in the analysis environment or on a Switch.

For installation and rollback instructions read README.md. Leave the configuration in diagnostics-only mode for the first offline Training Mode test. Initial HID ID 0 / fighter entry 0 mapping remains an assumption requiring physical testing. A successful build does not establish compatibility with Smash 13.0.5, correct EWGF timing, or compatibility with existing plugins.

Automatic DI follow-ups and Ferps-tech routes are not implemented. This is not an online automation release. No changes have been made to any Switch or SD card.
