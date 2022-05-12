fn main() {
    cc::Build::new()
        .file("fox32.c")
        .compile("fox32core_impl");

    bindgen::Builder::default()
        .header("fox32.h")
        .use_core()
        .ctypes_prefix("libc")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .generate()
        .expect("Failed to generate bindings")
        .write_to_file(std::path::PathBuf::from(std::env::var("OUT_DIR").unwrap()).join("bindings.rs"))
        .expect("Failed to write bindings");
}
