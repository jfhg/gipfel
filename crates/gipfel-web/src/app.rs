//! Root component: sidebar (open/fit/status/fit-toggle) + viewport canvas.
//! Owns the `Panorama` and the marker list; refits on every marker change.

use std::io::Read;
use std::rc::Rc;

use flate2::read::GzDecoder;
use gipfel_core::metadata::{apply_image_metadata, ImageMetaData};
use gipfel_core::panorama::Panorama;
use wasm_bindgen::closure::Closure;
use wasm_bindgen::JsCast;
use wasm_bindgen_futures::spawn_local;
use web_sys::{DragEvent, Event, HtmlCanvasElement, HtmlInputElement, MouseEvent};
use yew::prelude::*;

use crate::bridge;
use crate::canvas_view::{CanvasView, MarkerView, ViewCallbacks, ViewState};
use crate::markers::{self, Marker};

fn gunzip(gz: &[u8]) -> Result<Vec<u8>, String> {
    let mut raw = Vec::new();
    GzDecoder::new(gz)
        .read_to_end(&mut raw)
        .map(|_| raw)
        .map_err(|e| format!("gunzip: {e}"))
}

#[function_component(App)]
pub fn app() -> Html {
    let bitmap = use_state(|| None::<web_sys::ImageBitmap>);
    let view = use_state(ViewState::default);
    let status = use_state(|| "No image loaded.".to_string());
    let db_status = use_state(|| "Loading hill database…".to_string());
    let fit_info = use_state(String::new);
    let tick = use_state(|| 0u32);
    let canvas_ref = use_node_ref();
    let file_ref = use_node_ref();
    let pan = use_mut_ref(Panorama::new);
    let markers = use_state(Vec::<Marker>::new);
    let fit_distortion = use_state(|| false);
    let hover = use_state(|| Option::<String>::None);

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

    // load the hill database once at startup
    {
        let pan = pan.clone();
        let db_status = db_status.clone();
        use_effect_with((), move |_| {
            spawn_local(async move {
                let result = async {
                    let gz = bridge::fetch_bytes("gipfel.dat.gz").await?;
                    let raw = gunzip(&gz)?;
                    pan.borrow_mut()
                        .load_data_from_bytes(&raw)
                        .map_err(|e| format!("load: {e}"))
                }
                .await;
                match result {
                    Ok(()) => {
                        let n = pan.borrow().hills.len();
                        db_status.set(format!("Hill database: {n} peaks"));
                    }
                    Err(e) => db_status.set(format!("Hill database failed: {e}")),
                }
            });
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

    // refit the projection to the given markers and update the status line
    let do_refit = {
        let pan = pan.clone();
        let fit_info = fit_info.clone();
        let fit_distortion = fit_distortion.clone();
        Rc::new(move |mk: &[Marker]| {
            let info = {
                let mut p = pan.borrow_mut();
                match markers::refit(&mut p, mk, *fit_distortion) {
                    Ok(()) if mk.len() >= 4 => format!(
                        "fit: az {:.2}° · pitch {:.2}° · roll {:.2}° · scale {:.1}",
                        p.get_center_angle(),
                        p.get_pitch_angle(),
                        p.get_roll_angle(),
                        p.get_scale()
                    ),
                    Ok(()) => format!("{} markers — place at least 4 to fit", mk.len()),
                    Err(e) => format!("fit failed: {e}"),
                }
            };
            fit_info.set(info);
        })
    };

    // Decode selected/dropped bytes: display, read metadata, reset markers.
    let load_bytes = {
        let bitmap = bitmap.clone();
        let status = status.clone();
        let fit = fit.clone();
        let pan = pan.clone();
        let markers = markers.clone();
        let fit_info = fit_info.clone();
        Callback::from(move |(bytes, name): (Vec<u8>, String)| {
            let (bitmap, status, fit, pan, markers, fit_info) =
                (bitmap.clone(), status.clone(), fit.clone(), pan.clone(), markers.clone(), fit_info.clone());
            spawn_local(async move {
                match bridge::decode_bitmap(&bytes).await {
                    Ok(bmp) => {
                        let (w, h) = (bmp.width(), bmp.height());
                        let mut md = ImageMetaData::new();
                        md.load_image_from_bytes(&bytes);
                        let have = apply_image_metadata(&mut pan.borrow_mut(), &md, w, h);
                        let vis = pan.borrow().visible.len();
                        let vp = if have {
                            format!("viewpoint from image · {vis} peaks visible")
                        } else {
                            "no gipfel/EXIF viewpoint in this image".to_string()
                        };
                        status.set(format!("{name} — {w}×{h} px · {vp}"));
                        markers.set(Vec::new());
                        fit_info.set(String::new());
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
            match e
                .data_transfer()
                .and_then(|dt| dt.files())
                .and_then(|fl| fl.item(0))
            {
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

    let on_distortion_toggle = {
        let fit_distortion = fit_distortion.clone();
        let markers = markers.clone();
        let do_refit = do_refit.clone();
        Callback::from(move |e: Event| {
            let input: HtmlInputElement = e.target_unchecked_into();
            fit_distortion.set(input.checked());
            do_refit(&markers);
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
        click: {
            let pan = pan.clone();
            let markers = markers.clone();
            let view = view.clone();
            let status = status.clone();
            let do_refit = do_refit.clone();
            Callback::from(move |(ix, iy): (f64, f64)| {
                let p = pan.borrow_mut();
                match markers::nearest_hill(&p, ix, iy, 20.0 / view.scale) {
                    None => status.set(
                        "No peak near click — click on a green dot (zoom in to separate them)."
                            .to_string(),
                    ),
                    Some(idx) => {
                        let mut mk = (*markers).clone();
                        mk.retain(|m| m.hill != idx);
                        mk.push(Marker {
                            hill: idx,
                            x: ix,
                            y: iy,
                        });
                        drop(p);
                        do_refit(&mk);
                        markers.set(mk);
                    }
                }
            })
        },
        marker_drag: {
            let markers = markers.clone();
            Callback::from(move |(i, dx, dy): (usize, f64, f64)| {
                let mut mk = (*markers).clone();
                if let Some(m) = mk.get_mut(i) {
                    m.x += dx;
                    m.y += dy;
                }
                markers.set(mk);
            })
        },
        drag_end: {
            let markers = markers.clone();
            let do_refit = do_refit.clone();
            Callback::from(move |_: ()| do_refit(&markers))
        },
        context: {
            let markers = markers.clone();
            let view = view.clone();
            let status = status.clone();
            let do_refit = do_refit.clone();
            Callback::from(move |(ix, iy): (f64, f64)| {
                let mut mk = (*markers).clone();
                let before = mk.len();
                let r = 20.0 / view.scale;
                mk.retain(|m| (m.x - ix).hypot(m.y - iy) >= r);
                if mk.len() < before {
                    do_refit(&mk);
                    markers.set(mk);
                    status.set("Marker removed.".to_string());
                } else {
                    status.set("Right-click a marker to remove it.".to_string());
                }
            })
        },
        hover: {
            let pan = pan.clone();
            let view = view.clone();
            let hover = hover.clone();
            Callback::from(move |(ix, iy): (f64, f64)| {
                let p = pan.borrow();
                let name = markers::nearest_hill(&p, ix, iy, 20.0 / view.scale).map(|i| {
                    let h = &p.hills[i];
                    format!("{} · {:.0} m", h.name, h.height)
                });
                hover.set(name);
            })
        },
        hover_end: {
            let hover = hover.clone();
            Callback::from(move |_: ()| hover.set(None))
        },
    };

    // overlay data for the canvas
    let marker_views: Vec<MarkerView> = {
        let p = pan.borrow();
        markers
            .iter()
            .map(|m| MarkerView {
                x: m.x,
                y: m.y,
                name: p.hills[m.hill].name.clone(),
            })
            .collect()
    };
    let candidates: Vec<(f64, f64)> = {
        let p = pan.borrow();
        p.visible.iter().map(|&i| (p.hills[i].x, p.hills[i].y)).collect()
    };

    let zoom_pct = (view.scale * 100.0).round();
    let n_markers = markers.len();

    html! {
        <div class="app" ondrop={on_drop} ondragover={on_dragover}>
            <aside class="sidebar">
                <h1>{ "gipfel" }</h1>
                <p class="tag">{ "web · W2 markers" }</p>
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
                <p class="hint">{ (*db_status).clone() }</p>
                if bitmap.is_some() {
                    <label class="row">
                        <input type="checkbox" checked={*fit_distortion} onchange={on_distortion_toggle} />
                        { "Fit distortion (needs ≥ 4 markers)" }
                    </label>
                    <p class="hint">{ format!("{n_markers} markers · zoom {zoom_pct:.0} %") }</p>
                    if !fit_info.is_empty() {
                        <p class="fitinfo">{ (*fit_info).clone() }</p>
                    }
                    if let Some(h) = hover.as_ref() {
                        <p class="hover">{ h.clone() }</p>
                    }
                }
                <p class="hint">
                    { "Hover a green dot to see the peak · click to mark it · drag to adjust · right-click to remove." }
                </p>
                <p class="hint dim">
                    { "The view is fitted automatically once 4+ markers are placed." }
                </p>
            </aside>
            <main class="main">
                <CanvasView
                    bitmap={bitmap.as_ref().cloned()}
                    view={(*view).clone()}
                    {callbacks}
                    canvas_ref={canvas_ref.clone()}
                    tick={*tick}
                    markers={marker_views}
                    candidates={candidates}
                />
            </main>
        </div>
    }
}
