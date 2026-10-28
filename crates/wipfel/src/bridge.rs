//! Small JS helpers: window access, browser-native image decode, blob
//! reads, canvas context — the hand-written JS boundary of the app.

use js_sys::Uint8Array;
use wasm_bindgen::JsCast;
use wasm_bindgen_futures::JsFuture;
use web_sys::{Blob, CanvasRenderingContext2d, HtmlCanvasElement, ImageBitmap, Window};

pub fn window() -> Window {
    web_sys::window().expect("no global window")
}

/// Decode image bytes into a browser-native (GPU-backed) bitmap.
/// The browser does the JPEG decode and keeps the pixels where canvas
/// blitting is fast — cheaper than any pure-Rust path in wasm.
pub async fn decode_bitmap(bytes: &[u8]) -> Result<ImageBitmap, String> {
    let array = js_sys::Array::new();
    array.push(&Uint8Array::from(bytes).buffer());
    let blob = Blob::new_with_u8_array_sequence(&array).map_err(|_| "blob failed".to_string())?;
    let promise = window()
        .create_image_bitmap_with_blob(&blob)
        .map_err(|_| "createImageBitmap failed".to_string())?;
    let value = JsFuture::from(promise)
        .await
        .map_err(|e| format!("decode failed: {e:?}"))?;
    value
        .dyn_into::<ImageBitmap>()
        .map_err(|_| "decode produced non-bitmap".to_string())
}

/// Read a File/Blob fully into memory.
pub async fn read_blob(blob: &Blob) -> Result<Vec<u8>, String> {
    let buf = JsFuture::from(blob.array_buffer())
        .await
        .map_err(|e| format!("read failed: {e:?}"))?;
    Ok(Uint8Array::new(&buf).to_vec())
}

/// Fetch a URL (same-origin) into memory.
pub async fn fetch_bytes(url: &str) -> Result<Vec<u8>, String> {
    let resp = JsFuture::from(window().fetch_with_str(url))
        .await
        .map_err(|e| format!("fetch {url}: {e:?}"))?;
    let resp: web_sys::Response = resp
        .dyn_into()
        .map_err(|_| "bad response".to_string())?;
    if !resp.ok() {
        return Err(format!("{url}: HTTP {}", resp.status()));
    }
    let buf = JsFuture::from(resp.array_buffer().map_err(|_| "array_buffer".to_string())?)
        .await
        .map_err(|e| format!("body: {e:?}"))?;
    Ok(Uint8Array::new(&buf).to_vec())
}

/// setTimeout as an awaitable Promise.
pub fn delay_promise(ms: i32) -> js_sys::Promise {
    js_sys::Promise::new(&mut |resolve, _reject| {
        let _ = window().set_timeout_with_callback_and_timeout_and_arguments_0(&resolve, ms);
    })
}

pub fn ctx2d(canvas: &HtmlCanvasElement) -> Option<CanvasRenderingContext2d> {
    canvas
        .get_context("2d")
        .ok()
        .flatten()
        .and_then(|c| c.dyn_into::<CanvasRenderingContext2d>().ok())
}

/// Read a CSS custom property from :root (follows the active
/// prefers-color-scheme), with a fallback when unavailable.
pub fn css_var(name: &str, fallback: &str) -> String {
    window()
        .document()
        .and_then(|d| window().get_computed_style(&d.document_element()?).ok().flatten())
        .and_then(|s| s.get_property_value(name).ok())
        .map(|v| v.trim().to_string())
        .filter(|v| !v.is_empty())
        .unwrap_or_else(|| fallback.to_string())
}
