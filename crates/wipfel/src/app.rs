//! Root component: sidebar (open/fit/status/fit-toggle) + viewport canvas.
//! Owns the `Panorama` and the marker list; refits on every marker change.

use std::io::Read;
use std::rc::Rc;

use flate2::read::GzDecoder;
use gipfel_core::hill::{FLAG_DUPLIC, FLAG_HIDDEN, FLAG_TRACK_POINT, FLAG_VISIBLE};
use gipfel_core::track as gpx;
use gipfel_core::metadata::{apply_image_metadata, ImageMetaData};
use gipfel_core::panorama::Panorama;
use wasm_bindgen::closure::Closure;
use wasm_bindgen::JsCast;
use wasm_bindgen_futures::spawn_local;
use web_sys::{DragEvent, Event, HtmlCanvasElement, HtmlInputElement, InputEvent, MouseEvent};
use yew::prelude::*;

use crate::bridge;
use crate::canvas_view::{CanvasView, DotView, MarkerView, TrackView, ViewCallbacks, ViewState};
use crate::compass::{alph_at_img_x, Compass, CompassData};
use crate::markers::{self, Marker};
use crate::widgets::{NumItem, SliderItem};

/// sidebar-editable view parameters (degrees / meters / mm)
#[derive(Clone, Copy, PartialEq)]
struct UiParams {
    lat: f64,
    lon: f64,
    height: f64,
    az: f64,
    focal: f64,
    pitch: f64,
    roll: f64,
    /// visibility UI value: 0.11 - height_dist_ratio (right = more visible)
    vis: f64,
    label_font: f32,
}

impl Default for UiParams {
    fn default() -> Self {
        UiParams {
            lat: 0.0,
            lon: 0.0,
            height: 0.0,
            az: 0.0,
            focal: 50.0,
            pitch: 0.0,
            roll: 0.0,
            vis: 0.04,
            label_font: 12.0,
        }
    }
}

/// open Find Peak popup: anchor (screen + image coords) and search query
#[derive(Clone, PartialEq)]
struct FindPeak {
    sx: f64,
    sy: f64,
    ix: f64,
    iy: f64,
    query: String,
}

fn gunzip(gz: &[u8]) -> Result<Vec<u8>, String> {
    let mut raw = Vec::new();
    GzDecoder::new(gz)
        .read_to_end(&mut raw)
        .map(|_| raw)
        .map_err(|e| format!("gunzip: {e}"))
}

#[derive(Properties, PartialEq)]
struct FpProps {
    fp: FindPeak,
    items: Vec<(String, usize)>,
    on_pick: Callback<usize>,
    on_query: Callback<String>,
    on_close: Callback<()>,
}

/// searchable Find Peak dialog: type to filter, click to select
#[function_component(FindPeakPanel)]
fn find_peak_panel(p: &FpProps) -> Html {
    let input_ref = use_node_ref();
    // focus the search box when the dialog opens
    {
        let input_ref = input_ref.clone();
        use_effect_with((), move |_| {
            if let Some(i) = input_ref.cast::<HtmlInputElement>() {
                let _ = i.focus();
            }
        });
    }
    let on_query = {
        let on_query = p.on_query.clone();
        Callback::from(move |e: InputEvent| {
            let input: HtmlInputElement = e.target_unchecked_into();
            on_query.emit(input.value());
        })
    };
    let on_close = {
        let on_close = p.on_close.clone();
        Callback::from(move |_: MouseEvent| on_close.emit(()))
    };
    html! {
        <>
            <div class="fpbackdrop" onclick={on_close.clone()} />
            <div
                class="findpeak"
                style={format!("left: {}px; top: {}px", p.fp.sx + 10.0, p.fp.sy + 10.0)}
            >
                <div class="fphead">{ "Find Peak" }</div>
                <div class="fpsearch">
                    <input
                        type="text"
                        ref={input_ref}
                        placeholder="search…"
                        value={p.fp.query.clone()}
                        oninput={on_query}
                    />
                </div>
                <div class="fplist">
                    {
                        for p.items.iter().map(|(label, idx)| {
                            let onclick = {
                                let on_pick = p.on_pick.clone();
                                let i = *idx;
                                Callback::from(move |_: MouseEvent| on_pick.emit(i))
                            };
                            html! {
                                <button class="fpitem" {onclick}>{ label.clone() }</button>
                            }
                        })
                    }
                    if p.items.is_empty() {
                        <p class="fpempty">{ "no matches" }</p>
                    }
                </div>
                <div class="fpfoot">
                    <button class="fpcancel" onclick={on_close}>{ "Cancel" }</button>
                </div>
            </div>
        </>
    }
}

#[function_component(App)]
pub fn app() -> Html {
    let bitmap = use_state(|| None::<web_sys::ImageBitmap>);
    let view = use_state(ViewState::default);
    // Live view state for the event callbacks. A UseStateHandle cloned
    // into a callback derefs to its render-time snapshot (documented
    // Yew behaviour), so consecutive read-modify-writes within one
    // frame clobber each other — this silently killed pinch zoom, where
    // the pan emit reverted the zoom emit of the same event. The
    // mirror is the live source of truth; every write goes through
    // apply_view, which also pushes to the use_state for rendering.
    let view_live = use_mut_ref(ViewState::default);
    let apply_view: Rc<dyn Fn(&dyn Fn(&mut ViewState))> = {
        let view = view.clone();
        let view_live = view_live.clone();
        Rc::new(move |f: &dyn Fn(&mut ViewState)| {
            let mut v = (*view_live.borrow()).clone();
            f(&mut v);
            *view_live.borrow_mut() = v.clone();
            view.set(v);
        })
    };
    let status = use_state(|| "No image loaded".to_string());
    let db_status = use_state(|| "Loading hill database…".to_string());
    let fit_info = use_state(String::new);
    let tick = use_state(|| 0u32);
    let canvas_ref = use_node_ref();
    let file_ref = use_node_ref();
    let pan = use_mut_ref(Panorama::new);
    let markers = use_state(Vec::<Marker>::new);
    let fit_distortion = use_state(|| false);
    let hover = use_state(|| Option::<String>::None);
    let vp = use_state(UiParams::default);
    let img_size = use_state(|| (0u32, 0u32));
    let find_peak = use_state(|| Option::<FindPeak>::None);
    let show_hidden = use_state(|| false);
    let peak_labels = use_state(|| true);
    // collapsible control panel (desktop nav bar behaviour): start
    // collapsed on narrow screens
    let nav_open = use_state(|| {
        bridge::window()
            .inner_width()
            .ok()
            .and_then(|w| w.as_f64())
            .unwrap_or(1200.0)
            >= 900.0
    });
    // hill indices of the GPS track, in file order
    let track = use_state(Vec::<usize>::new);
    let track_width = use_state(|| 200.0f64);
    let track_file_ref = use_node_ref();
    // set once the user pans or zooms by hand; auto-fit is then suppressed
    // until the next image load
    let user_moved = use_mut_ref(|| false);

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
        let canvas_ref = canvas_ref.clone();
        let apply = apply_view.clone();
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
            apply(&move |v| {
                v.scale = scale;
                v.ox = (cw - iw * scale) / 2.0;
                v.oy = (ch - ih * scale) / 2.0;
            });
        })
    };

    // Keep the fit correct even when layout was not ready at load time:
    // a ResizeObserver fires once with the element's initial size right
    // after it starts observing, then on every later resize (window
    // resize, sidebar collapse, ...). Always bump the redraw tick so the
    // canvas backing store tracks its CSS size — otherwise the browser
    // stretches the stale bitmap — and additionally re-fit whenever the
    // user has not panned or zoomed by hand since the last image load
    // (this is also what makes the very first image appear).
    {
        let canvas_ref = canvas_ref.clone();
        let fit = fit.clone();
        let bitmap = bitmap.clone();
        let user_moved = user_moved.clone();
        let tick = tick.clone();
        use_effect_with((*bitmap).is_some(), move |has_image| {
            let observer = if *has_image {
                canvas_ref
                    .cast::<HtmlCanvasElement>()
                    .and_then(|c| {
                        let fit2 = fit.clone();
                        let um = user_moved.clone();
                        let tick2 = tick.clone();
                        let closure = wasm_bindgen::closure::Closure::wrap(Box::new(
                            move |_entries: js_sys::Array, _obs: web_sys::ResizeObserver| {
                                tick2.set(*tick2 + 1);
                                if !*um.borrow() {
                                    fit2.emit(MouseEvent::new("fit").unwrap());
                                }
                            },
                        ) as Box<dyn Fn(js_sys::Array, web_sys::ResizeObserver)>);
                        let ro =
                            web_sys::ResizeObserver::new(closure.as_ref().unchecked_ref()).ok()?;
                        let _ = ro.observe(&c);
                        // the observer does not own the closure; keep both
                        // alive until this effect is torn down
                        Some((ro, closure))
                    })
            } else {
                None
            };
            move || drop(observer)
        });
    }

    // refit the projection to the given markers and update the status line
    let do_refit = {
        let pan = pan.clone();
        let fit_info = fit_info.clone();
        let fit_distortion = fit_distortion.clone();
        Rc::new(move |mk: &[Marker]| {
            if mk.is_empty() {
                fit_info.set(String::new());
                return;
            }
            let info = {
                let mut p = pan.borrow_mut();
                match markers::refit(&mut p, mk, *fit_distortion) {
                    Ok(()) => format!(
                        "fit: az {:.2}° · pitch {:.2}° · roll {:.2}° · scale {:.1}",
                        p.get_center_angle(),
                        p.get_pitch_angle(),
                        p.get_roll_angle(),
                        p.get_scale()
                    ),
                    Err(e) => format!("fit failed: {e}"),
                }
            };
            fit_info.set(info);
        })
    };

    // place a marker for a hill at an image position and refit
    let mark_peak = {
        let markers = markers.clone();
        let do_refit = do_refit.clone();
        Callback::from(move |(idx, ix, iy): (usize, f64, f64)| {
            let mut mk = (*markers).clone();
            mk.retain(|m| m.hill != idx);
            mk.push(Marker { hill: idx, x: ix, y: iy });
            do_refit(&mk);
            markers.set(mk);
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
        let vp = vp.clone();
        let img_size = img_size.clone();
        let find_peak = find_peak.clone();
        let user_moved = user_moved.clone();
        Callback::from(move |(bytes, name): (Vec<u8>, String)| {
            let (bitmap, status, fit, pan, markers, fit_info) =
                (bitmap.clone(), status.clone(), fit.clone(), pan.clone(), markers.clone(), fit_info.clone());
            let vp = vp.clone();
            let img_size = img_size.clone();
            let find_peak = find_peak.clone();
            let user_moved = user_moved.clone();
            spawn_local(async move {
                match bridge::decode_bitmap(&bytes).await {
                    Ok(bmp) => {
                        let (w, h) = (bmp.width(), bmp.height());
                        let mut md = ImageMetaData::new();
                        md.load_image_from_bytes(&bytes);
                        let have = apply_image_metadata(&mut pan.borrow_mut(), &md, w, h);
                        let vis = pan.borrow().visible.len();
                        let u = {
                            let p = pan.borrow();
                            UiParams {
                                lat: p.get_view_lat(),
                                lon: p.get_view_long(),
                                height: p.get_view_height(),
                                az: p.get_center_angle().rem_euclid(360.0),
                                focal: (p.get_scale() * 35.0 / w as f64).clamp(1.0, 300.0),
                                pitch: p.get_pitch_angle(),
                                roll: p.get_roll_angle(),
                                vis: (0.11 - p.height_dist_ratio).clamp(0.01, 0.1),
                                label_font: vp.label_font,
                            }
                        };
                        vp.set(u);
                        img_size.set((w, h));
                        let vp = if have {
                            format!("viewpoint from image · {vis} peaks visible")
                        } else {
                            "no gipfel/EXIF viewpoint in this image".to_string()
                        };
                        status.set(format!("{name} — {w}×{h} px · {vp}"));
                        markers.set(Vec::new());
                        fit_info.set(String::new());
                        find_peak.set(None);
                        *user_moved.borrow_mut() = false;
                        bitmap.set(Some(bmp));
                        fit.emit(MouseEvent::new("fit").unwrap());
                        // same nudge as the desktop app: re-fit shortly
                        // after the first paint so the image is guaranteed
                        // to appear even if layout was not ready yet
                        {
                            let fit = fit.clone();
                            spawn_local(async move {
                                let _ = wasm_bindgen_futures::JsFuture::from(
                                    bridge::delay_promise(120),
                                )
                                .await;
                                fit.emit(MouseEvent::new("fit").unwrap());
                            });
                        }
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

    let open_track_picker = {
        let track_file_ref = track_file_ref.clone();
        Callback::from(move |_: MouseEvent| {
            if let Some(input) = track_file_ref.cast::<HtmlInputElement>() {
                input.click();
            }
        })
    };

    // parse a GPX file and swap it into the panorama as track-point hills
    // (always appended last, so marker hill indices never shift)
    let on_track_file_change = {
        let pan = pan.clone();
        let track = track.clone();
        let status = status.clone();
        Callback::from(move |e: Event| {
            let input: HtmlInputElement = e.target_unchecked_into();
            let Some(file) = input.files().and_then(|f| f.item(0)) else {
                return;
            };
            let (pan, track, status) = (pan.clone(), track.clone(), status.clone());
            let name = file.name();
            spawn_local(async move {
                match bridge::read_blob(&file).await {
                    Ok(bytes) => match gpx::load_gpx_from_bytes(&bytes, 2000) {
                        Ok(hills) => {
                            let n = hills.len();
                            {
                                let p = &mut *pan.borrow_mut();
                                p.remove_hills(FLAG_TRACK_POINT);
                                p.add_hills(&hills);
                                track.set((p.hills.len() - n..p.hills.len()).collect());
                            }
                            status.set(format!("Track {name}: {n} points"));
                        }
                        Err(err) => status.set(format!("GPX failed: {err}")),
                    },
                    Err(err) => status.set(format!("Failed to read {name}: {err}")),
                }
            });
            input.set_value(""); // allow re-selecting the same file
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

    // sidebar parameter editing: update UI state and apply to the panorama
    macro_rules! param_cb {
        ($field:ident, $apply:expr) => {{
            let vp = vp.clone();
            let pan = pan.clone();
            Callback::from(move |v: f64| {
                let mut u = *vp;
                u.$field = v;
                vp.set(u);
                let f: &dyn Fn(&mut Panorama, f64) = &$apply;
                f(&mut pan.borrow_mut(), v);
            })
        }};
    }
    let on_lat = param_cb!(lat, |p: &mut Panorama, v| p.set_view_lat(v));
    let on_lon = param_cb!(lon, |p, v| p.set_view_long(v));
    let on_height = param_cb!(height, |p, v| p.set_view_height(v));
    let on_az = param_cb!(az, |p, v| p.set_center_angle(v));
    let on_pitch = param_cb!(pitch, |p, v| p.set_pitch_angle(v));
    let on_roll = param_cb!(roll, |p, v| p.set_roll_angle(v));
    let on_vis = param_cb!(vis, |p, v| p.set_height_dist_ratio(0.11 - v));
    let on_focal = {
        let vp = vp.clone();
        let pan = pan.clone();
        let img_size = img_size.clone();
        Callback::from(move |v: f64| {
            let mut u = *vp;
            u.focal = v;
            vp.set(u);
            let w = img_size.0 as f64;
            if w > 0.0 {
                pan.borrow_mut().set_scale(v * w / 35.0);
            }
        })
    };
    let on_font = {
        let vp = vp.clone();
        Callback::from(move |v: f64| {
            let mut u = *vp;
            u.label_font = v as f32;
            vp.set(u);
        })
    };
    let on_show_hidden = {
        let show_hidden = show_hidden.clone();
        Callback::from(move |e: Event| {
            let input: HtmlInputElement = e.target_unchecked_into();
            show_hidden.set(input.checked());
        })
    };
    let on_peak_labels = {
        let peak_labels = peak_labels.clone();
        Callback::from(move |e: Event| {
            let input: HtmlInputElement = e.target_unchecked_into();
            peak_labels.set(input.checked());
        })
    };
    let on_track_width = {
        let track_width = track_width.clone();
        Callback::from(move |v: f64| track_width.set(v))
    };

    // compass strip: shares the image transform, drag changes azimuth
    let compass_data = {
        let p = pan.borrow();
        CompassData {
            ptype: p.projection_type,
            parms: p.parms,
            center_deg: p.get_center_angle(),
            zoom: view.scale,
            cx: view.ox + img_size.0 as f64 * view.scale / 2.0,
            max_view_angle: p.max_view_angle,
        }
    };
    let compass_change = {
        let pan = pan.clone();
        let vp = vp.clone();
        Callback::from(move |deg: f64| {
            pan.borrow_mut().set_center_angle(deg);
            let mut u = *vp;
            u.az = deg.rem_euclid(360.0);
            vp.set(u);
        })
    };

    let fp_close = {
        let find_peak = find_peak.clone();
        Callback::from(move |_: ()| find_peak.set(None))
    };

    // Find Peak picker: searchable list over the geographically close hills
    // (same candidate set and filtering as the desktop dialog)
    let findpeak_popup = if let Some(fp) = &*find_peak {
        let items: Vec<(String, usize)> = {
            let p = pan.borrow();
            let q = fp.query.to_uppercase();
            let mut entries: Vec<(String, usize)> = p
                .close
                .iter()
                .filter(|&&i| {
                    let h = &p.hills[i];
                    h.flags & (FLAG_DUPLIC | FLAG_TRACK_POINT) == 0
                        && (q.is_empty() || h.name.to_uppercase().contains(&q))
                })
                .map(|&i| {
                    (
                        format!("{} ({} m)", p.hills[i].name, p.hills[i].height as i64),
                        i,
                    )
                })
                .collect();
            entries.sort_by(|a, b| a.0.cmp(&b.0));
            entries.truncate(300);
            entries
        };
        let on_pick = {
            let mark = mark_peak.clone();
            let find_peak = find_peak.clone();
            let (ix, iy) = (fp.ix, fp.iy);
            Callback::from(move |hill: usize| {
                mark.emit((hill, ix, iy));
                find_peak.set(None);
            })
        };
        let on_query = {
            let find_peak = find_peak.clone();
            Callback::from(move |q: String| {
                if let Some(mut fp) = (*find_peak).clone() {
                    fp.query = q;
                    find_peak.set(Some(fp));
                }
            })
        };
        html! {
            <FindPeakPanel
                fp={(*fp).clone()}
                {items}
                {on_pick}
                {on_query}
                on_close={fp_close.clone()}
            />
        }
    } else {
        Html::default()
    };

    let callbacks = ViewCallbacks {
        pan: {
            let apply = apply_view.clone();
            let user_moved = user_moved.clone();
            Callback::from(move |(dx, dy): (f64, f64)| {
                *user_moved.borrow_mut() = true;
                apply(&move |v| {
                    v.ox += dx;
                    v.oy += dy;
                });
            })
        },
        zoom: {
            let apply = apply_view.clone();
            let user_moved = user_moved.clone();
            Callback::from(move |(cx, cy, factor): (f64, f64, f64)| {
                *user_moved.borrow_mut() = true;
                apply(&move |v| {
                    let ns = (v.scale * factor).clamp(0.02, 64.0);
                    let k = ns / v.scale;
                    v.ox = cx - (cx - v.ox) * k;
                    v.oy = cy - (cy - v.oy) * k;
                    v.scale = ns;
                });
            })
        },
        pinch: {
            let apply = apply_view.clone();
            let user_moved = user_moved.clone();
            Callback::from(move |(cx, cy, factor, dx, dy): (f64, f64, f64, f64, f64)| {
                *user_moved.borrow_mut() = true;
                // zoom and pan in ONE update: two separate updates would
                // each start from the same stale snapshot and clobber
                apply(&move |v| {
                    let ns = (v.scale * factor).clamp(0.02, 64.0);
                    let k = ns / v.scale;
                    v.ox = cx - (cx - v.ox) * k + dx;
                    v.oy = cy - (cy - v.oy) * k + dy;
                    v.scale = ns;
                });
            })
        },
        click: {
            let pan = pan.clone();
            let view = view.clone();
            let mark = mark_peak.clone();
            let find_peak = find_peak.clone();
            let img_size = img_size.clone();
            let show_hidden = show_hidden.clone();
            Callback::from(move |(ix, iy): (f64, f64)| {
                let scale = view.scale;
                // unambiguous click right on a peak: mark directly
                // (borrow must be released before mark -> do_refit!)
                let near = {
                    let p = pan.borrow();
                    markers::nearest_hill(&p, ix, iy, 10.0 / scale, *show_hidden)
                };
                if let Some(idx) = near {
                    find_peak.set(None);
                    mark.emit((idx, ix, iy));
                    return;
                }
                // otherwise open the Find Peak picker (searchable, like the
                // desktop dialog): the correct peak may project far away
                // while the viewpoint is still rough
                let (w, h) = (img_size.0 as f64, img_size.1 as f64);
                find_peak.set(Some(FindPeak {
                    sx: (ix + w / 2.0) * scale + view.ox,
                    sy: (iy + h / 2.0) * scale + view.oy,
                    ix,
                    iy,
                    query: String::new(),
                }));
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
            let show_hidden = show_hidden.clone();
            Callback::from(move |(ix, iy): (f64, f64)| {
                let p = pan.borrow();
                let name = markers::nearest_hill(&p, ix, iy, 20.0 / view.scale, *show_hidden)
                    .map(|i| {
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

    // Lens FOV visibility limit, port of the desktop bump(): the angle at
    // which the (possibly distorted) projection reaches the image edge.
    // Peaks beyond it cannot be inside the photo even if the fitted
    // distortion folds their projection back into the image. Running this
    // every render is idempotent and cheap; visibility is only rebuilt
    // when the limit actually changed.
    {
        let mut p = pan.borrow_mut();
        let limit = if img_size.0 > 0 {
            let c = p.parms.a_center;
            let proj = gipfel_core::projection::projection_from_type(p.projection_type);
            let a_edge = alph_at_img_x(p.projection_type, &*proj, &p.parms, img_size.0 as f64 / 2.0, c);
            (a_edge - c).abs()
        } else {
            0.0
        };
        if (limit - p.max_view_angle).abs() > 1e-9 {
            p.max_view_angle = limit;
            let known: Vec<usize> = markers.iter().map(|m| m.hill).collect();
            p.update_visible_mountains(Some(&known));
        }
    }

    // overlay data for the canvas
    let marker_views: Vec<MarkerView> = {
        let p = pan.borrow();
        markers
            .iter()
            .map(|m| MarkerView {
                hill: m.hill,
                x: m.x,
                y: m.y,
                name: p.hills[m.hill].name.clone(),
            })
            .collect()
    };
    let candidates: Vec<DotView> = {
        let p = pan.borrow();
        let marked: Vec<usize> = markers.iter().map(|m| m.hill).collect();
        p.visible
            .iter()
            .filter(|&&i| {
                p.hills[i].flags & (FLAG_DUPLIC | FLAG_TRACK_POINT) == 0
                    && (*show_hidden || p.hills[i].flags & FLAG_HIDDEN == 0)
            })
            .map(|&i| DotView {
                x: p.hills[i].x,
                y: p.hills[i].y,
                hidden: p.hills[i].flags & FLAG_HIDDEN != 0,
                marked: marked.contains(&i),
                name: p.hills[i].name.clone(),
            })
            .collect()
    };
    let track_views: Vec<TrackView> = {
        let p = pan.borrow();
        track
            .iter()
            .filter(|&&i| p.hills[i].flags & FLAG_VISIBLE != 0)
            .map(|&i| {
                let h = &p.hills[i];
                // C: scale * track_width / (dist_m * 10), distance in meters
                let dist_m = p.get_real_distance(i).max(1.0);
                TrackView {
                    x: h.x,
                    y: h.y,
                    hidden: h.flags & FLAG_HIDDEN != 0,
                    width: (p.get_scale() * *track_width / (dist_m * 10.0)).clamp(1.0, 1000.0),
                }
            })
            .collect()
    };

    let collapse_cb = {
        let nav_open = nav_open.clone();
        Callback::from(move |_: MouseEvent| nav_open.set(false))
    };
    let expand_cb = {
        let nav_open = nav_open.clone();
        Callback::from(move |_: MouseEvent| nav_open.set(true))
    };

    let zoom_pct = (view.scale * 100.0).round();
    let n_markers = markers.len();

    html! {
        <div class="app" ondrop={on_drop} ondragover={on_dragover}>
            <aside class={if *nav_open { "sidebar" } else { "sidebar collapsed" }}>
                <button class="collapse-btn" title="Hide controls"
                    onclick={collapse_cb}>{ "◀" }</button>
                <h1>{ "wipfel" }</h1>
                <p class="tag">{ "web gipfel photogrammetry" }</p>
                <a class="moreinfo" href="https://flpsed.org/gipfel.html" target="_blank"
                    rel="noopener noreferrer">{ "More information…" }</a>
                <button onclick={open_picker}>{ "Open image…" }</button>
                <button onclick={fit} disabled={bitmap.is_none()}>{ "Fit to window" }</button>
                <input
                    type="file"
                    ref={file_ref}
                    accept="image/*"
                    class="hidden-file"
                    onchange={on_file_change}
                />
                <input
                    type="file"
                    ref={track_file_ref}
                    accept=".gpx,application/gpx+xml"
                    class="hidden-file"
                    onchange={on_track_file_change}
                />
                <p class="status">{ (*status).clone() }</p>
                <p class="hint">{ (*db_status).clone() }</p>
                if bitmap.is_some() {
                    <>
                        <div class="section">{ "Viewpoint" }</div>
                        <NumItem label="Latitude" value={vp.lat} step={0.00001} decimals={5}
                            onchange={on_lat} />
                        <NumItem label="Longitude" value={vp.lon} step={0.00001} decimals={5}
                            onchange={on_lon} />
                        <NumItem label="Height (m)" value={vp.height} step={1.0} decimals={0}
                            onchange={on_height} />
                        <div class="section">{ "Orientation" }</div>
                        <SliderItem label="Azimuth" display={format!("{:.1}°", vp.az)}
                            value={vp.az} min={0.0} max={360.0} step={0.01} onchange={on_az} />
                        <SliderItem label="Focal (35mm)" display={format!("{:.1} mm", vp.focal)}
                            value={vp.focal} min={1.0} max={300.0} step={0.01} onchange={on_focal} />
                        <SliderItem label="Pitch" display={format!("{:.1}°", vp.pitch)}
                            value={vp.pitch} min={-40.0} max={20.0} step={0.01} onchange={on_pitch} />
                        <SliderItem label="Roll" display={format!("{:.1}°", vp.roll)}
                            value={vp.roll} min={-10.0} max={10.0} step={0.01} onchange={on_roll} />
                        <div class="section">{ "Projection" }</div>
                        <label class="row">
                            <input type="checkbox" checked={*fit_distortion} onchange={on_distortion_toggle} />
                            { "Fit distortion (needs ≥ 4 markers)" }
                        </label>
                        <div class="section">{ "Display" }</div>
                        <label class="row">
                            <input type="checkbox" checked={*show_hidden} onchange={on_show_hidden} />
                            { "Show hidden peaks" }
                        </label>
                        <label class="row">
                            <input type="checkbox" checked={*peak_labels} onchange={on_peak_labels} />
                            { "Show peak labels" }
                        </label>
                        <SliderItem label="Visibility" display={format!("{:.0} %", (vp.vis - 0.01) / 0.09 * 100.0)}
                            value={vp.vis} min={0.01} max={0.1} step={0.001} onchange={on_vis} />
                        <SliderItem label="Label size" display={format!("{:.0} px", vp.label_font)}
                            value={vp.label_font as f64} min={8.0} max={28.0} step={1.0} onchange={on_font} />
                        <p class="hint">{ format!("{n_markers} markers · zoom {zoom_pct:.0} %") }</p>
                        if !fit_info.is_empty() {
                            <p class="fitinfo">{ (*fit_info).clone() }</p>
                        }
                        if let Some(h) = hover.as_ref() {
                            <p class="hover">{ h.clone() }</p>
                        }
                    </>
                }
                <p class="hint">
                    { "Hover a dot to see the peak · click to mark it (or pick from the list) · drag to adjust · right-click to remove." }
                </p>
                <p class="hint dim">
                    { "The view is fitted as soon as markers are placed; more markers improve the fit." }
                </p>
                <div class="section track-section">{ "Track" }</div>
                <button onclick={open_track_picker}>{ "Load GPX Track…" }</button>
                if !track.is_empty() {
                    <SliderItem label="Track width" display={format!("{:.0} m", *track_width)}
                        value={*track_width} min={50.0} max={1000.0} step={10.0}
                        onchange={on_track_width} />
                }
            </aside>
            <main class="main">
                <div class="canvaswrap">
                    if !*nav_open {
                        <button class="nav-toggle" title="Show controls"
                            onclick={expand_cb}>{ "☰" }</button>
                    }
                    <div class="canvasstatus">{ (*status).clone() }</div>
                    <CanvasView
                        bitmap={bitmap.as_ref().cloned()}
                        view={(*view).clone()}
                        {callbacks}
                        canvas_ref={canvas_ref.clone()}
                        tick={*tick}
                        markers={marker_views}
                        track={track_views}
                        candidates={candidates}
                        peak_labels={*peak_labels}
                        label_font={vp.label_font}
                    />
                    { findpeak_popup.clone() }
                </div>
                if bitmap.is_some() {
                    <Compass data={compass_data} tick={*tick} on_change={compass_change} />
                }
            </main>
        </div>
    }
}
