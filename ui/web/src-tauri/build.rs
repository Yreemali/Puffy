use std::env;
use std::fs;
use std::path::{Path, PathBuf};

fn runtime_library(target_os: &str) -> &'static str {
    match target_os {
        "windows" => "puffy_native.dll",
        "macos" => "libpuffy_native.dylib",
        _ => "libpuffy_native.so",
    }
}

fn copy_runtime_library(native_dir: &Path, target_os: &str) {
    let Some(profile_dir) = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR"))
        .ancestors().nth(3).map(Path::to_path_buf) else { return };
    let source = native_dir.join(runtime_library(target_os));
    if source.exists() {
        let _ = fs::copy(source, profile_dir.join(runtime_library(target_os)));
    }
}

fn main() {
    tauri_build::build();
    let root = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("manifest dir"));
    let target_os = env::var("CARGO_CFG_TARGET_OS").expect("target OS");
    let native_dir = env::var("PUFFY_NATIVE_LIB_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|_| root.join("native"));
    println!("cargo:rustc-link-search=native={}", native_dir.display());
    println!("cargo:rustc-link-lib=dylib=puffy_native");
    if target_os == "linux" {
        println!("cargo:rustc-link-arg=-Wl,-rpath,{}", native_dir.display());
        println!("cargo:rustc-link-arg=-Wl,-rpath,$ORIGIN");
        println!("cargo:rustc-link-arg=-Wl,-rpath,$ORIGIN/../lib/Puffy");
    } else if target_os == "macos" {
        println!("cargo:rustc-link-arg=-Wl,-rpath,{}", native_dir.display());
        println!("cargo:rustc-link-arg=-Wl,-rpath,@executable_path/../Frameworks");
    }
    copy_runtime_library(&native_dir, &target_os);
    println!("cargo:rerun-if-changed={}", native_dir.join(runtime_library(&target_os)).display());
    println!("cargo:rerun-if-env-changed=PUFFY_NATIVE_LIB_DIR");
}
