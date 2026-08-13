use std::env;
use std::path::PathBuf;

fn main() {
    tauri_build::build();
    let root = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("manifest dir"));
    let native_dir = env::var("PUFFY_NATIVE_LIB_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|_| root.join("../../../build-native-check"));
    println!("cargo:rustc-link-search=native={}", native_dir.display());
    println!("cargo:rustc-link-lib=dylib=puffy_native");
    println!("cargo:rustc-link-arg=-Wl,-rpath,{}", native_dir.display());
    println!("cargo:rerun-if-env-changed=PUFFY_NATIVE_LIB_DIR");
}
