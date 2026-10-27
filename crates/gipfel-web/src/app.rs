//! Root component: sidebar (open/fit/status) + viewport canvas.
//! File loading: `<input type=file>` and drag-and-drop, decoded by the
//! browser into an ImageBitmap (see `bridge`).

use std::rc::Rc;
use wasm_bindgen::closure::Closure;
use wasm_bindgen::JsCast;
use wasm_bindgen_futures::spawn_local;
use web_sys::{DragEvent, Event, HtmlCanvasElement, HtmlInputElement, MouseEvent};
use yew::prelude::*;

use crate::bridge;
use crate::canvas_view::{CanvasView, ViewCallbacks, ViewState};

#[function_component(App)]
pub fn app() -> Html {
    let bitmap = use_state(|| None::<web_sys::ImageBitmap>);
    let view = use_state(ViewState::default);
    let status = use_state(|| "No image loaded.".to_string());
    let tick = use_state(|| 0u32);
    let canvas_ref = use_node_ref();
    let file_ref = use_node_ref();

    // force a redraw when the window is resized so the canvas backing
    // store tracks its CSS size (listener lives as long as the app)
    {
        let tick = tick.clone();
        use_effect_with((), move |_| {
            let closure = Closure::wrap(Box::new(move || tick.set(*tick + 1)) as Box<dyn Fn()>);
            let _ = bridge::window()
                .add_event_listener_with_callback("resize", closure.as_ref().unchecked_ref());
            // the listener is never removed (app lifetime); leak the closure
            std::mem::forget(closure);
        });
    }

    // Fit the current bitmap into the canvas, centered.
    let fit = {
        let bitmap = bitmap.clone();
        let view = view.clone();
        let canvas_ref = canvas_ref.clone();
        Callback::from(move |_: MouseEvent| {
            let Some(bmp) = bitmap.as_ref() else { return };
            let (cw, ch) = canvas_ref
                .cast::<HtmlCanvasElement>()
                .map(|c| (c.client_width() as f64, c.client_height() as f64))
                .unwrap_or((800.0, 600.0));
            let (iw, ih) = (bmp.width() as f64, bmp.height() as f64);
            if iw <= 0.0 || ih <= 0.0 || cw <= 0.0 || ch <= 0.0 {
                return;
            }
            let scale = (cw / iw).min(ch / ih);
            view.set(ViewState {
                scale,
                ox: (cw - iw * scale) / 2.0,
                oy: (ch - ih * scale) / 2.0,
            });
        })
    };

    // Decode dropped/selected bytes and display them, fitting to view.
    let load_bytes = {
        let bitmap = bitmap.clone();
        let status = status.clone();
        let fit = fit.clone();
        Callback::from(move |(bytes, name): (Vec<u8>, String)| {
            let (bitmap, status, fit) = (bitmap.clone(), status.clone(), fit.clone());
            spawn_local(async move {
                match bridge::decode_bitmap(&bytes).await {
                    Ok(bmp) => {
                        status.set(format!(
                            "{name} — {}×{} px",
                            bmp.width(),
                            bmp.height()
                        ));
                        bitmap.set(Some(bmp));
                        fit.emit(MouseEvent::new("fit").unwrap());
                    }
                    Err(e) => status.set(format!("Failed to load {name}: {e}")),
                }
            });
        })
    };

    let read_file = {
        let load_bytes = load_bytes.clone();
        let status = status.clone();
        Rc::new(move |file: web_sys::File| {
            let (load_bytes, status) = (load_bytes.clone(), status.clone());
            let name = file.name();
            spawn_local(async move {
                match bridge::read_blob(&file).await {
                    Ok(bytes) => load_bytes.emit((bytes, name)),
                    Err(e) => status.set(format!("Failed to read {name}: {e}")),
                }
            });
        })
    };

    let on_file_change = {
        let read_file = read_file.clone();
        Callback::from(move |e: Event| {
            let input: HtmlInputElement = e.target_unchecked_into();
            if let Some(file) = input.files().and_then(|fl| fl.item(0)) {
                read_file(file);
            }
        })
    };

    let on_drop = {
        let read_file = read_file.clone();
        let status = status.clone();
        Callback::from(move |e: DragEvent| {
            e.prevent_default();
            match e.data_transfer().and_then(|dt| dt.files()).and_then(|fl| fl.item(0)) {
                Some(file) => read_file(file),
                None => status.set("Dropped item was not a file.".to_string()),
            }
        })
    };
    let on_dragover = Callback::from(|e: DragEvent| e.prevent_default());

    let open_picker = {
        let file_ref = file_ref.clone();
        Callback::from(move |_: MouseEvent| {
            if let Some(input) = file_ref.cast::<HtmlInputElement>() {
                input.click();
            }
        })
    };

    let callbacks = ViewCallbacks {
        pan: {
            let view = view.clone();
            Callback::from(move |(dx, dy): (f64, f64)| {
                let mut v = (*view).clone();
                v.ox += dx;
                v.oy += dy;
                view.set(v);
            })
        },
        zoom: {
            let view = view.clone();
            Callback::from(move |(cx, cy, factor): (f64, f64, f64)| {
                let v = &*view;
                let ns = (v.scale * factor).clamp(0.02, 64.0);
                let k = ns / v.scale;
                view.set(ViewState {
                    scale: ns,
                    ox: cx - (cx - v.ox) * k,
                    oy: cy - (cy - v.oy) * k,
                });
            })
        },
    };

    let zoom_pct = (view.scale * 100.0).round();

    html! {
        <div class="app" ondrop={on_drop} ondragover={on_dragover}>
            <aside class="sidebar">
                <h1>{ "gipfel" }</h1>
                <p class="tag">{ "web · W1 scaffold" }</p>
                <button onclick={open_picker}>{ "Open image…" }</button>
                <button onclick={fit} disabled={bitmap.is_none()}>{ "Fit to window" }</button>
                <input
                    type="file"
                    ref={file_ref}
                    accept="image/jpeg,image/png"
                    class="hidden-file"
                    onchange={on_file_change}
                />
                <p class="status">{ (*status).clone() }</p>
                if bitmap.is_some() {
                    <p class="hint">{ format!("zoom: {zoom_pct:.0} %") }</p>
                }
                <p class="hint">{ "Drag to pan, wheel to zoom. Drop a JPEG anywhere." }</p>
                <p class="hint dim">
                    { "Peak identification, fitting and saving arrive in W2/W3." }
                </p>
            </aside>
            <main class="main">
                <CanvasView
                    bitmap={bitmap.as_ref().cloned()}
                    view={(*view).clone()}
                    {callbacks}
                    canvas_ref={canvas_ref.clone()}
                    tick={*tick}
                />
            </main>
        </div>
    }
}
