//! Root component: sidebar (open/fit/status/fit-toggle) + viewport canvas.
//! Owns the `Panorama` and the marker list; refits on every marker change.

use std::io::Read;
use std::rc::Rc;

use flate2::read::GzDecoder;
use gipfel_core::hill::{FLAG_DUPLIC, FLAG_HIDDEN, FLAG_TRACK_POINT, FLAG_VISIBLE};
use gipfel_core::track as gpx;
use gipfel_core::metadata::{apply_image_metadata, ImageMetaData};
use gipfel_core::panorama::Panorama;
use gipfel_core::projection::ProjectionType;
use wasm_bindgen::closure::Closure;
use wasm_bindgen::JsCast;
use wasm_bindgen_futures::spawn_local;
use web_sys::{DragEvent, Event, HtmlCanvasElement, HtmlInputElement, InputEvent, MouseEvent};
use yew::prelude::*;

use crate::bridge;
use crate::canvas_view::{CanvasView, DotView, MarkerView, TrackView, ViewCallbacks, ViewState};
use crate::compass::{alph_at_img_x, Compass, CompassData};
use crate::markers::{self, Marker};
use crate::widgets::{NumItem, SliderItem, TextItem};

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

/// view parameter overrides from the URL (degrees / mm), applied on
/// top of whatever the image metadata provides
#[derive(Clone, Copy, Default)]
struct Overrides {
    az: Option<f64>,
    pitch: Option<f64>,
    roll: Option<f64>,
    focal: Option<f64>,
    vis: Option<f64>,
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

/// Build stamp injected by serve-here.sh / build-release.sh so a
/// quick glance at the sidebar reveals whether the browser is running
/// a cached old bundle.
const BUILD_STAMP: &str = match option_env!("STAMP") {
    Some(s) => s,
    None => "dev",
};

/// Read a query parameter from the page URL (e.g. `/?file=test.jpg`).
fn url_param(name: &str) -> Option<String> {
    let search = bridge::window().location().search().ok()?;
    let params = web_sys::UrlSearchParams::new_with_str(&search).ok()?;
    params.get(name)
}

fn gunzip(gz: &[u8]) -> Result<Vec<u8>, String> {
    let mut raw = Vec::new();
    GzDecoder::new(gz)
        .read_to_end(&mut raw)
        .map(|_| raw)
        .map_err(|e| format!("gunzip: {e}"))
}

/// localStorage key holding the distortion profile store, in the same
/// FLTK prefs format the desktop app uses on disk (so files can be
/// exported/imported between the two).
const PREFS_KEY: &str = "gipfel-distortion-profiles";

fn load_prefs() -> Vec<(String, gipfel_core::profile::Profile)> {
    bridge::store_get(PREFS_KEY)
        .map(|t| gipfel_core::profile::parse_prefs(&t))
        .unwrap_or_default()
}

fn save_prefs(list: &[(String, gipfel_core::profile::Profile)]) -> Result<(), String> {
    bridge::store_set(PREFS_KEY, &gipfel_core::profile::serialize_prefs(list))
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
    // 0 = rectilinear, 1 = cylindrical (panoramic); mirrors
    // Panorama::projection_type, synced from the image comment on load
    let projection = use_state(|| 0u32);
    let hover = use_state(|| Option::<String>::None);
    let vp = use_state(UiParams::default);
    let img_size = use_state(|| (0u32, 0u32));
    let find_peak = use_state(|| Option::<FindPeak>::None);
    let show_hidden = use_state(|| false);
    let peak_labels = use_state(|| true);
    // collapsible control panel (desktop nav bar behaviour): open at
    // start so the image picker is reachable even on phones; it
    // collapses automatically once an image is loaded on a narrow screen
    let nav_open = use_state(|| true);
    // hill indices of the GPS track, in file order
    let track = use_state(Vec::<usize>::new);
    let track_width = use_state(|| 200.0f64);
    let track_file_ref = use_node_ref();
    // optional alternative hill database (e.g. a regional list)
    let db_file_ref = use_node_ref();
    // set once the user pans or zooms by hand; auto-fit is then suppressed
    // until the next image load
    let user_moved = use_mut_ref(|| false);
    // view parameter overrides from the URL query string
    let overrides = use_mut_ref(Overrides::default);
    // original bytes + name of the loaded image, kept for "Save image"
    // (the annotated JPEG is the source image plus a gipfel comment)
    let orig = use_mut_ref(|| Option::<(Vec<u8>, String)>::None);
    // EXIF metadata of the loaded image (camera name for profile matching)
    let img_md = use_mut_ref(ImageMetaData::new);
    // distortion profile store (localStorage, FLTK prefs format)
    let profile_name = use_state(String::new);
    let profiles = use_state(Vec::<String>::new);
    let profile_overwrite = use_mut_ref(|| false);

    // force a redraw when the window is resized so the canvas backing
    // store tracks its CSS size (listener lives as long as the app).
    // The live counter lives in a RefCell: a UseStateHandle cloned into
    // these persistent closures would deref to its render-time snapshot
    // and stall the bump after the first resize.
    let tick_live = use_mut_ref(|| 0u32);
    {
        let tick = tick.clone();
        let live = tick_live.clone();
        use_effect_with((), move |_| {
            let closure = Closure::wrap(Box::new(move || {
                let v = {
                    let mut l = live.borrow_mut();
                    *l += 1;
                    *l
                };
                tick.set(v);
            }) as Box<dyn Fn()>);
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
                    // aapt gunzips .gz assets, so an APK ships the plain
                    // gipfel.dat while the web dist ships gipfel.dat.gz
                    let raw = match bridge::fetch_bytes("gipfel.dat.gz").await {
                        Ok(gz) => gunzip(&gz)?,
                        Err(_) => bridge::fetch_bytes("gipfel.dat").await?,
                    };
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
                        let live2 = tick_live.clone();
                        let closure = wasm_bindgen::closure::Closure::wrap(Box::new(
                            move |_entries: js_sys::Array, _obs: web_sys::ResizeObserver| {
                                let v = {
                                    let mut l = live2.borrow_mut();
                                    *l += 1;
                                    *l
                                };
                                tick2.set(v);
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
        let vp = vp.clone();
        let img_size = img_size.clone();
        Rc::new(move |mk: &[Marker]| {
            if mk.is_empty() {
                fit_info.set(String::new());
                return;
            }
            let info = {
                let mut p = pan.borrow_mut();
                match markers::refit(&mut p, mk, *fit_distortion) {
                    Ok(()) => {
                        // push the fitted values into the slider state,
                        // otherwise Azimuth/Pitch/Roll/Focal keep showing
                        // the pre-fit numbers
                        let mut u = *vp;
                        u.az = p.get_center_angle().rem_euclid(360.0);
                        u.pitch = p.get_pitch_angle();
                        u.roll = p.get_roll_angle();
                        let w = img_size.0.max(1) as f64;
                        u.focal = (p.get_scale() * 36.0 / w).clamp(1.0, 300.0);
                        vp.set(u);
                        format!(
                            "fit: az {:.2}° · pitch {:.2}° · roll {:.2}° · scale {:.1}",
                            p.get_center_angle(),
                            p.get_pitch_angle(),
                            p.get_roll_angle(),
                            p.get_scale()
                        )
                    }
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
        let nav_open = nav_open.clone();
        let overrides = overrides.clone();
        let orig = orig.clone();
        let img_md = img_md.clone();
        let profile_name = profile_name.clone();
        let profiles = profiles.clone();
        let projection = projection.clone();
        Callback::from(move |(bytes, name): (Vec<u8>, String)| {
            let (bitmap, status, fit, pan, markers, fit_info) =
                (bitmap.clone(), status.clone(), fit.clone(), pan.clone(), markers.clone(), fit_info.clone());
            let vp = vp.clone();
            let img_size = img_size.clone();
            let find_peak = find_peak.clone();
            let user_moved = user_moved.clone();
            let nav_open = nav_open.clone();
            let overrides = overrides.clone();
            let orig = orig.clone();
            let img_md = img_md.clone();
            let profile_name = profile_name.clone();
            let profiles = profiles.clone();
            let projection = projection.clone();
            spawn_local(async move {
                match bridge::decode_bitmap(&bytes).await {
                    Ok(bmp) => {
                        // narrow screens: give the canvas the room once
                        // the setup phase is over
                        let narrow = bridge::window()
                            .inner_width()
                            .ok()
                            .and_then(|w| w.as_f64())
                            .unwrap_or(1200.0)
                            < 900.0;
                        if narrow {
                            nav_open.set(false);
                        }
                        let (w, h) = (bmp.width(), bmp.height());
                        let mut md = ImageMetaData::new();
                        md.load_image_from_bytes(&bytes);
                        let have = apply_image_metadata(&mut pan.borrow_mut(), &md, w, h);
                        // explicit URL parameters win over the metadata,
                        // e.g. /?file=img.jpg&pitch=-20 for a photo whose
                        // EXIF lacks (or fakes) the camera orientation
                        {
                            let ov = *overrides.borrow();
                            let mut p = pan.borrow_mut();
                            if let Some(v) = ov.az {
                                p.set_center_angle(v);
                            }
                            if let Some(v) = ov.pitch {
                                p.set_pitch_angle(v);
                            }
                            if let Some(v) = ov.roll {
                                p.set_roll_angle(v);
                            }
                            if let Some(v) = ov.focal {
                                p.set_scale(v * w.max(h) as f64 / 36.0);
                            }
                            if let Some(v) = ov.vis {
                                // same mapping as the Visibility slider
                                p.set_height_dist_ratio((0.11 - v).clamp(0.01, 0.1));
                            }
                            // bring the lens-FOV limit up to date NOW (the
                            // render-time sync block would only apply it a
                            // frame later, and the peak count below depends
                            // on it)
                            p.max_view_angle = if p.projection_type == ProjectionType::Cylindrical {
                                0.0
                            } else {
                                let proj = gipfel_core::projection::projection_from_type(p.projection_type);
                                let c = p.parms.a_center;
                                (alph_at_img_x(
                                    p.projection_type,
                                    &*proj,
                                    &p.parms,
                                    w as f64 / 2.0,
                                    c,
                                ) - c)
                                    .abs()
                            };
                            // initial visibility: pick the ratio that puts
                            // a reasonable number of peaks in the view,
                            // unless the URL pinned ?vis=
                            if ov.vis.is_none() {
                                p.auto_visibility(8, 40);
                            }
                        }
                        // keep the source bytes and EXIF for "Save image"
                        *orig.borrow_mut() = Some((bytes.clone(), name.clone()));
                        *img_md.borrow_mut() = md.clone();
                        // distortion profile: prefill the name field with
                        // the camera/focal auto-match and apply it when a
                        // stored profile of that name exists (desktop does
                        // the same on image load)
                        {
                            let auto = gipfel_core::metadata::distortion_profile_name(&md);
                            profile_name.set(auto.clone().unwrap_or_default());
                            let all = load_prefs();
                            profiles.set(all.iter().map(|(n, _)| n.clone()).collect());
                            if let Some(n) = &auto {
                                if let Some((_, pr)) = all.iter().find(|(m, _)| m == n) {
                                    pan.borrow_mut().set_distortion_params(pr.k0, pr.k1, pr.x0);
                                }
                            }
                        }
                        let vis = pan.borrow().visible.len();
                        let u = {
                            let p = pan.borrow();
                            UiParams {
                                lat: p.get_view_lat(),
                                lon: p.get_view_long(),
                                height: p.get_view_height(),
                                az: p.get_center_angle().rem_euclid(360.0),
                                focal: (p.get_scale() * 36.0 / w as f64).clamp(1.0, 300.0),
                                pitch: p.get_pitch_angle(),
                                roll: p.get_roll_angle(),
                                vis: (0.11 - p.height_dist_ratio).clamp(0.01, 0.1),
                                label_font: vp.label_font,
                            }
                        };
                        vp.set(u);
                        // a saved gipfel comment may request the panoramic
                        // projection; sync the dropdown with the result
                        projection.set(pan.borrow().projection_type as u32);
                        // force the per-render limit block to re-evaluate:
                        // its change detection compares against the old
                        // value, and a metadata-driven switch to cylindrical
                        // (limit 0) would otherwise look "unchanged" and
                        // keep the stale visibility list
                        pan.borrow_mut().max_view_angle = -1.0;
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

    // Auto-load an image given as ?file=<path or http(s) URL> so a photo
    // can be opened by link alone, e.g. /?file=test.jpg or
    // /?file=https://example.org/pano.jpg&az=57.4. It goes through the
    // normal load pipeline, so EXIF viewpoint/focal metadata is applied
    // exactly as for a picked file. External URLs need CORS on their
    // host; the status line reports failures.
    {
        let load_bytes = load_bytes.clone();
        let status = status.clone();
        let overrides = overrides.clone();
        use_effect_with((), move |_| {
            let Some(path) = url_param("file") else { return };
            let external = path.starts_with("https://") || path.starts_with("http://");
            if path.is_empty() || (!external && path.contains(":/")) {
                status.set("?file= must be a path on this host or an http(s) URL".to_string());
                return;
            }
            let name = path.rsplit('/').next().unwrap_or(&path).to_string();
            *overrides.borrow_mut() = Overrides {
                az: url_param("az").and_then(|s| s.parse().ok()),
                pitch: url_param("pitch").and_then(|s| s.parse().ok()),
                roll: url_param("roll").and_then(|s| s.parse().ok()),
                focal: url_param("focal").and_then(|s| s.parse().ok()),
                vis: url_param("vis").and_then(|s| s.parse().ok()),
            };
            let (load_bytes, status) = (load_bytes.clone(), status.clone());
            let path2 = path.clone();
            spawn_local(async move {
                match bridge::fetch_bytes(&path).await {
                    Ok(bytes) => load_bytes.emit((bytes, name)),
                    Err(e) => status.set(format!("Could not fetch {path2}: {e}")),
                }
            });
        });
    }

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

    let open_db_picker = {
        let db_file_ref = db_file_ref.clone();
        Callback::from(move |_: MouseEvent| {
            if let Some(input) = db_file_ref.cast::<HtmlInputElement>() {
                input.click();
            }
        })
    };

    // replace the hill database at runtime (e.g. everest.dat for the
    // Khumbu); markers and track reference hill indices and are dropped
    let on_db_file_change = {
        let pan = pan.clone();
        let db_status = db_status.clone();
        let markers = markers.clone();
        let track = track.clone();
        Callback::from(move |e: Event| {
            let input: HtmlInputElement = e.target_unchecked_into();
            let Some(file) = input.files().and_then(|f| f.item(0)) else {
                return;
            };
            let (pan, db_status, markers, track) = (
                pan.clone(),
                db_status.clone(),
                markers.clone(),
                track.clone(),
            );
            let name = file.name();
            spawn_local(async move {
                match bridge::read_blob(&file).await {
                    Ok(bytes) => {
                        // accept plain .dat and gzipped variants
                        let raw = match bytes.first() {
                            Some(0x1f) if bytes.get(1) == Some(&0x8b) => match gunzip(&bytes) {
                                Ok(r) => r,
                                Err(err) => {
                                    db_status.set(format!("gunzip failed: {err}"));
                                    return;
                                }
                            },
                            _ => bytes,
                        };
                        // NOTE: the borrow must end before the match arm,
                        // which calls pan.borrow() itself (nested borrow
                        // would panic and trap the whole wasm module)
                        let res = pan.borrow_mut().load_data_from_bytes(&raw);
                        match res {
                            Ok(()) => {
                                markers.set(Vec::new());
                                track.set(Vec::new());
                                let n = pan.borrow().hills.len();
                                db_status.set(format!("Hill database {name}: {n} peaks"));
                            }
                            Err(err) => db_status.set(format!("Database failed: {err}")),
                        }
                    }
                    Err(err) => db_status.set(format!("Failed to read {name}: {err}")),
                }
            });
            input.set_value(""); // allow re-selecting the same file
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

    // "Save image": re-encode the original JPEG with the current fit as
    // gipfel comment (same bytes + format as the desktop app writes),
    // delivered as a browser download.
    let on_save_image = {
        let orig = orig.clone();
        let img_md = img_md.clone();
        let pan = pan.clone();
        let img_size = img_size.clone();
        let status = status.clone();
        Callback::from(move |_: MouseEvent| {
            let Some((bytes, name)) = orig.borrow().clone() else {
                status.set("No image loaded.".to_string());
                return;
            };
            // mirror of the desktop md_from_state; the comment keeps the
            // historical scale * 35 / max(w,h) focal encoding so saved
            // files stay interchangeable with the desktop versions
            let mut out = ImageMetaData::new();
            {
                let m = img_md.borrow();
                out.manufacturer = m.manufacturer.clone();
                out.model = m.model.clone();
            }
            {
                let p = pan.borrow();
                out.longitude = p.get_view_long();
                out.latitude = p.get_view_lat();
                out.height = p.get_view_height();
                out.direction = p.get_center_angle();
                out.pitch = p.get_pitch_angle();
                out.roll = p.get_roll_angle();
                out.focal_length_35mm =
                    p.get_scale() * 35.0 / (img_size.0.max(img_size.1) as f64).max(1.0);
                out.projection_type = p.projection_type as i32;
                out.k0 = p.parms.k0;
                out.k1 = p.parms.k1;
                out.x0 = p.parms.x0;
            }
            match out.save_image_to_bytes(&bytes) {
                Ok(jpg) => {
                    let stem = name
                        .rsplit_once('.')
                        .map(|(s, _)| s.to_string())
                        .unwrap_or_else(|| name.clone());
                    let outname = format!("{stem}-gipfel.jpg");
                    match bridge::download(&outname, &jpg, "image/jpeg") {
                        Ok(()) => status.set(format!("Saved {outname} (browser download).")),
                        Err(e) => status.set(format!("Download failed: {e}")),
                    }
                }
                Err(e) => status.set(format!("Save failed: {e} (input must be JPEG)")),
            }
        })
    };

    // distortion profiles: localStorage store in FLTK prefs format
    let refresh_profiles = {
        let profiles = profiles.clone();
        Rc::new(move || {
            profiles.set(load_prefs().into_iter().map(|(n, _)| n).collect())
        })
    };
    let apply_profile = {
        let pan = pan.clone();
        let status = status.clone();
        Rc::new(move |name: &str| {
            match load_prefs().into_iter().find(|(n, _)| n == name) {
                Some((_, p)) => {
                    pan.borrow_mut().set_distortion_params(p.k0, p.k1, p.x0);
                    status.set(format!("Loaded distortion profile '{name}'."));
                }
                None => status.set(format!("Profile '{name}' not found.")),
            }
        })
    };
    let on_profile_name = {
        let profile_name = profile_name.clone();
        let profile_overwrite = profile_overwrite.clone();
        Callback::from(move |e: InputEvent| {
            let input: HtmlInputElement = e.target_unchecked_into();
            profile_name.set(input.value());
            *profile_overwrite.borrow_mut() = false;
        })
    };
    let on_profile_save = {
        let profile_name = profile_name.clone();
        let pan = pan.clone();
        let status = status.clone();
        let profile_overwrite = profile_overwrite.clone();
        let refresh = refresh_profiles.clone();
        Callback::from(move |_: MouseEvent| {
            let name = profile_name.trim().to_string();
            if name.is_empty() {
                status.set("Enter a profile name.".into());
                return;
            }
            if let Err(e) = gipfel_core::profile::sanitize_name(&name) {
                status.set(format!("Invalid profile name: {e}"));
                return;
            }
            let prof = {
                let p = pan.borrow();
                gipfel_core::profile::Profile {
                    k0: p.parms.k0,
                    k1: p.parms.k1,
                    x0: p.parms.x0,
                }
            };
            let mut all = load_prefs();
            let exists = all.iter().any(|(n, _)| *n == name);
            if exists && !*profile_overwrite.borrow() {
                *profile_overwrite.borrow_mut() = true;
                status.set(format!("Profile '{name}' exists — press Save again to overwrite."));
                return;
            }
            all.retain(|(n, _)| *n != name);
            all.push((name.clone(), prof));
            match save_prefs(&all) {
                Ok(()) => {
                    *profile_overwrite.borrow_mut() = false;
                    refresh();
                    status.set(format!("Saved distortion profile '{name}'."));
                }
                Err(e) => status.set(format!("Profile save failed: {e}")),
            }
        })
    };
    let on_profile_load = {
        let profile_name = profile_name.clone();
        let apply = apply_profile.clone();
        Callback::from(move |_: MouseEvent| apply(profile_name.trim()))
    };
    let on_profile_select = {
        let profile_name = profile_name.clone();
        let apply = apply_profile.clone();
        Callback::from(move |e: Event| {
            let sel: web_sys::HtmlSelectElement = e.target_unchecked_into();
            let name = sel.value();
            profile_name.set(name.clone());
            apply(&name);
        })
    };

    let on_projection = {
        let pan = pan.clone();
        let projection = projection.clone();
        let tick = tick.clone();
        let markers = markers.clone();
        let do_refit = do_refit.clone();
        Callback::from(move |e: Event| {
            let sel: web_sys::HtmlSelectElement = e.target_unchecked_into();
            let cyl = sel.value() == "1";
            projection.set(if cyl { 1 } else { 0 });
            pan.borrow_mut().set_projection(if cyl {
                ProjectionType::Cylindrical
            } else {
                ProjectionType::Rectilinear
            });
            // View parameters are projection-specific (a cylindrical scale
            // is linear in azimuth, a rectilinear one is tangent-based), so
            // refit to the markers after switching. Without this the old
            // projection's parameters get reinterpreted by the new one and
            // the next marker drag refits garbage, polluting the shared
            // parameters for BOTH projections until another refit saves it.
            do_refit(&markers);
            // The peak coordinates change with the projection (the FOV-limit
            // sync in the render body rebuilds them), but no CanvasView prop
            // changes - bump the redraw tick or the canvas keeps showing the
            // stale paint until a marker drag forces a repaint.
            tick.set(*tick + 1);
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
    // GPS paste field: "47.676,13.32042" — the format gallery apps show
    // for coordinates; height stays a separate field
    let on_gps = {
        let vp = vp.clone();
        let pan = pan.clone();
        let status = status.clone();
        Callback::from(move |text: String| {
            let parts: Vec<f64> = text
                .replace([';', ' '], ",")
                .split(',')
                .filter(|p| !p.trim().is_empty())
                .filter_map(|p| p.trim().parse::<f64>().ok())
                .collect();
            if parts.len() < 2 || parts[0].abs() > 90.0 || parts[1].abs() > 180.0 {
                status.set("GPS: expected \"lat,lon\" like 47.676,13.32042".to_string());
                return;
            }
            let mut u = *vp;
            u.lat = parts[0];
            u.lon = parts[1];
            let mut p = pan.borrow_mut();
            p.set_view_lat(parts[0]);
            p.set_view_long(parts[1]);
            vp.set(u);
            status.set(format!("Viewpoint: {:.5}, {:.5}", parts[0], parts[1]));
        })
    };
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
                pan.borrow_mut().set_scale(v * w / 36.0);
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
                        format!(
                            "{} · {:.0} m · distance {:.2} km",
                            h.name,
                            h.height,
                            p.get_real_distance(i) / 1000.0
                        )
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
    // distortion folds their projection back into the image. Cylindrical
    // (panoramic) images are exempt: they may cover the full 360°, so the
    // limit is set to 0 = unlimited. Running this every render is
    // idempotent and cheap; visibility is only rebuilt when the limit
    // actually changed.
    {
        let mut p = pan.borrow_mut();
        let limit = if img_size.0 == 0 || p.projection_type == ProjectionType::Cylindrical {
            // 0 = unlimited: no image loaded, or panoramic projection,
            // whose image may wrap all the way around the viewpoint
            0.0
        } else {
            let c = p.parms.a_center;
            let proj = gipfel_core::projection::projection_from_type(p.projection_type);
            let a_edge = alph_at_img_x(p.projection_type, &*proj, &p.parms, img_size.0 as f64 / 2.0, c);
            (a_edge - c).abs()
        };
        if (limit - p.max_view_angle).abs() > 1e-9 {
            p.max_view_angle = limit;
            let known: Vec<usize> = markers.iter().map(|m| m.hill).collect();
            p.update_visible_mountains(Some(&known));
        }
        // ?debug=1 dumps the effective visibility state to the browser
        // console on every render (DevTools → Console)
        if url_param("debug").is_some() {
            web_sys::console::log_1(
                &format!(
                    "gipfel dbg: proj={:?} max_view_angle={:.4} visible={} img={}x{}",
                    p.projection_type,
                    p.max_view_angle,
                    p.visible.len(),
                    img_size.0,
                    img_size.1
                )
                .into(),
            );
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

    // Google-Maps-style +/- buttons: zoom about the canvas center using
    // the same math as wheel/pinch zoom; counts as manual navigation so
    // the auto-fit stays out of the way afterwards
    let zoom_step = {
        let apply = apply_view.clone();
        let user_moved = user_moved.clone();
        let canvas_ref = canvas_ref.clone();
        Rc::new(move |factor: f64| {
            let (cx, cy) = canvas_ref
                .cast::<HtmlCanvasElement>()
                .map(|c| {
                    (
                        c.client_width() as f64 / 2.0,
                        c.client_height() as f64 / 2.0,
                    )
                })
                .unwrap_or((400.0, 300.0));
            *user_moved.borrow_mut() = true;
            apply(&move |v| {
                let ns = (v.scale * factor).clamp(0.02, 64.0);
                let k = ns / v.scale;
                v.ox = cx - (cx - v.ox) * k;
                v.oy = cy - (cy - v.oy) * k;
                v.scale = ns;
            });
        })
    };
    let zoom_in = {
        let zoom = zoom_step.clone();
        Callback::from(move |_: MouseEvent| zoom(1.4))
    };
    let zoom_out = {
        let zoom = zoom_step.clone();
        Callback::from(move |_: MouseEvent| zoom(1.0 / 1.4))
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
                <h1>{ "gipfel" }</h1>
                <a class="tag" href="https://flpsed.org/gipfel.html" target="_blank"
                    rel="noopener noreferrer">{ "Photogrammetry for Mountain Images" }</a>
                <button onclick={open_picker}>{ "Open image…" }</button>
                if bitmap.is_some() {
                    <button onclick={fit}>{ "Fit to window" }</button>
                    <button onclick={on_save_image}>{ "Save image…" }</button>
                }
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
                        <TextItem label="GPS paste" placeholder="47.676,13.32042"
                            onchange={on_gps} />
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
                        <select class="textinput" onchange={on_projection}>
                            <option value="0" selected={*projection == 0}>{ "Normal (rectilinear)" }</option>
                            <option value="1" selected={*projection == 1}>{ "Panorama (cylindrical)" }</option>
                        </select>
                        // Distortion only affects the rectilinear
                        // projection: the cylindrical formulas do not
                        // contain k0/k1/x0 and its derivative trait
                        // defaults are NaN, so a level-2 fit is a provable
                        // no-op there. Grey the whole group out in
                        // panorama mode (native fieldset disabling).
                        <fieldset class="distortion" disabled={*projection == 1}>
                        <label class="row">
                            <input type="checkbox" checked={*fit_distortion} onchange={on_distortion_toggle} />
                            { "Fit distortion (needs ≥ 4 markers)" }
                        </label>
                        <div class="subsection">{ "Distortion profiles" }</div>
                        <input
                            type="text"
                            class="textinput"
                            placeholder="profile name"
                            value={(*profile_name).clone()}
                            oninput={on_profile_name}
                        />
                        <div class="row gap">
                            <button onclick={on_profile_save}>{ "Save" }</button>
                            <button onclick={on_profile_load}>{ "Load" }</button>
                        </div>
                        if !profiles.is_empty() {
                            <select class="textinput" onchange={on_profile_select}>
                                <option value="" selected={true}>{ "saved profiles…" }</option>
                                {
                                    profiles.iter().map(|n| html! {
                                        <option value={n.clone()} key={n.clone()}>{ n.clone() }</option>
                                    }).collect::<Html>()
                                }
                            </select>
                        }
                        </fieldset>
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
                        <div class="section">{ "Track" }</div>
                        <button onclick={open_track_picker}>{ "Load GPX Track…" }</button>
                        if !track.is_empty() {
                            <SliderItem label="Track width" display={format!("{:.0} m", *track_width)}
                                value={*track_width} min={50.0} max={1000.0} step={10.0}
                                onchange={on_track_width} />
                        }
                        // deliberately below the track controls: the
                        // default database is loaded automatically, this
                        // is only an optional replacement
                        <button class="secondary" onclick={open_db_picker}>
                            { "Load alternative hill database…" }
                        </button>
                        <input
                            type="file"
                            class="hidden-file"
                            accept=".dat,.gz,.gzip,application/gzip"
                            ref={db_file_ref}
                            onchange={on_db_file_change}
                        />
                        // last so revealing it on hover cannot shift any
                        // control above it (the bottom-pinned infofoot
                        // absorbs the height change)
                        if let Some(h) = hover.as_ref() {
                            <p class="hover">{ h.clone() }</p>
                        }
                    </>
                }
                <div class="infofoot">
                    <p class="hint dim">{ format!("gipfel {} · build {}", env!("CARGO_PKG_VERSION"), BUILD_STAMP) }</p>
                    <p class="hint">
                        { "Hover a dot to see the peak · click to mark it (or pick from the list) · drag to adjust · right-click to remove." }
                    </p>
                    <p class="hint dim">
                        { "The view is fitted as soon as markers are placed; more markers improve the fit." }
                    </p>
                </div>
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
                    if bitmap.is_some() {
                        <div class="zoomctrl">
                            <button onclick={zoom_in} title="Zoom in" aria-label="Zoom in">{"+"}</button>
                            <button onclick={zoom_out} title="Zoom out" aria-label="Zoom out">{"\u{2212}"}</button>
                        </div>
                    }
                </div>
                if bitmap.is_some() {
                    <Compass data={compass_data} tick={*tick} on_change={compass_change} />
                }
            </main>
        </div>
    }
}
