use std::{env, path::PathBuf, process::Command};
fn main() {
    println!("cargo:rerun-if-changed=core/ewgf_core.c");
    println!("cargo:rerun-if-changed=core/ewgf_core.h");
    println!("cargo:rerun-if-env-changed=KAZUYA_CLANG");
    let target=env::var("TARGET").expect("Cargo TARGET");
    if !target.starts_with("aarch64") {
        panic!("Native plugin must be built with cargo skyline build --release. Host tests use tools/test_core.sh.");
    }
    let out=PathBuf::from(env::var_os("OUT_DIR").expect("Cargo OUT_DIR")).join("ewgf_core.o");
    let cc=env::var("KAZUYA_CLANG").unwrap_or_else(|_| "clang".into());
    let status=Command::new(&cc).args([
        "--target=aarch64-unknown-none", "-std=c11", "-O2", "-fPIC",
        "-ffreestanding", "-fno-stack-protector", "-fno-builtin",
        "-Wall", "-Wextra", "-Werror", "-Icore", "-c", "core/ewgf_core.c", "-o"
    ]).arg(&out).status().expect("LLVM clang is required. Install LLVM and put clang on PATH, or set KAZUYA_CLANG to its executable.");
    assert!(status.success(), "The AArch64 input-core build failed");
    println!("cargo:rustc-link-arg={}",out.display());
}
