//! gipfel desktop shell: a webview window around the static WASM build
//! in `dist/` (produced by build-frontend.sh). All real work happens in
//! the wasm front end; this binary only hosts it.

#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

fn main() {
    tauri::Builder::default()
        .run(tauri::generate_context!())
        .expect("error while running gipfel");
}
