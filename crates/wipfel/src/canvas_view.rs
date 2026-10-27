//! Viewport canvas: browser-native bitmap blitted with a pan/zoom
//! transform, plus the marker overlay and its interactions.

use web_sys::{HtmlCanvasElement, ImageBitmap, MouseEvent, PointerEvent, WheelEvent};
use yew::prelude::*;

use crate::bridge;

/// Image -> screen transform: `screen = img * scale + (ox, oy)`.
/// Same convention as the desktop viewport widget.
#[derive(Clone, Debug, PartialEq, Default)]
pub struct ViewState {
    pub scale: f64,
    pub ox: f64,
    pub oy: f64,
}

/// A marker in center-relative image coordinates.
#[derive(Clone, Debug, PartialEq)]
pub struct MarkerView {
    pub x: f64,
    pub y: f64,
    pub name: String,
}

#[derive(Clone, PartialEq)]
pub struct ViewCallbacks {
    pub pan: Callback<(f64, f64)>,
    /// (cursor x, cursor y, factor) — zoom anchored at the cursor.
    pub zoom: Callback<(f64, f64, f64)>,
    /// click on empty space: (x, y) center-relative image coords
    pub click: Callback<(f64, f64)>,
    /// drag a marker: (marker index, dx, dy) in image coords
    pub marker_drag: Callback<(usize, f64, f64)>,
    /// a marker drag finished (time to refit)
    pub drag_end: Callback<()>,
    /// right-click: (x, y) center-relative image coords
    pub context: Callback<(f64, f64)>,
    /// pointer moved over the image (not dragging): (x, y) center-relative
    pub hover: Callback<(f64, f64)>,
    /// pointer left the canvas
    pub hover_end: Callback<()>,
}

#[derive(Properties, PartialEq)]
pub struct Props {
    pub bitmap: Option<ImageBitmap>,
    pub view: ViewState,
    pub callbacks: ViewCallbacks,
    pub canvas_ref: NodeRef,
    /// bumped on window resize to force a redraw
    pub tick: u32,
    pub markers: Vec<MarkerView>,
    /// projected positions of all visible hills (guide dots) with their
    /// hidden-by-occlusion flag
    pub candidates: Vec<(f64, f64, bool)>,
    /// marker label font size, screen px
    pub label_font: f32,
}

#[derive(Clone, PartialEq)]
enum DragMode {
    Pan,
    Marker(usize),
}

#[derive(PartialEq)]
struct DragState {
    mode: Option<DragMode>,
    last: (f64, f64),
    start: (f64, f64),
    moved: bool,
}

#[function_component(CanvasView)]
pub fn canvas_view(props: &Props) -> Html {
    let drag = use_mut_ref(|| DragState {
        mode: None,
        last: (0.0, 0.0),
        start: (0.0, 0.0),
        moved: false,
    });

    // redraw on every view/bitmap/marker change and on resize ticks
    {
        let bitmap = props.bitmap.clone();
        let view = props.view.clone();
        let canvas_ref = props.canvas_ref.clone();
        let markers = props.markers.clone();
        let candidates = props.candidates.clone();
        let label_font = props.label_font;
        use_effect_with(
            (bitmap, view, props.tick, markers.clone(), candidates.clone(), label_font),
            move |(bitmap, view, _tick, markers, candidates, label_font)| {
                let Some(canvas) = canvas_ref.cast::<HtmlCanvasElement>() else {
                    return;
                };
                let Some(ctx) = bridge::ctx2d(&canvas) else {
                    return;
                };
                let dpr = bridge::window().device_pixel_ratio();
                let cw = canvas.client_width() as f64;
                let ch = canvas.client_height() as f64;
                // keep the backing store in sync with the CSS size
                let bw = (cw * dpr).max(1.0) as u32;
                let bh = (ch * dpr).max(1.0) as u32;
                if canvas.width() != bw || canvas.height() != bh {
                    canvas.set_width(bw);
                    canvas.set_height(bh);
                }
                let _ = ctx.set_transform(1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
                // web-sys 0.3: set_fill_style takes a JsValue (deprecated alias)
                #[allow(deprecated)]
                ctx.set_fill_style(&wasm_bindgen::JsValue::from_str("#101014"));
                ctx.fill_rect(0.0, 0.0, canvas.width() as f64, canvas.height() as f64);
                let Some(bitmap) = bitmap else { return };
                let _ = ctx.set_transform(
                    dpr * view.scale,
                    0.0,
                    0.0,
                    dpr * view.scale,
                    dpr * view.ox,
                    dpr * view.oy,
                );
                let _ = ctx.draw_image_with_image_bitmap(bitmap, 0.0, 0.0);

                // overlays in screen space
                let _ = ctx.set_transform(dpr, 0.0, 0.0, dpr, 0.0, 0.0);
                let hw = bitmap.width() as f64 / 2.0;
                let hh = bitmap.height() as f64 / 2.0;
                let sx = |x: f64| (x + hw) * view.scale + view.ox;
                let sy = |y: f64| (y + hh) * view.scale + view.oy;

                // guide dots for all visible peaks: dark outline ring +
                // bright core so they stay visible on any photo content;
                // peaks hidden by nearer terrain are drawn grey
                #[allow(deprecated)]
                ctx.set_fill_style(&wasm_bindgen::JsValue::from_str("rgba(0, 0, 0, 0.85)"));
                for (cx, cy, _hidden) in candidates {
                    let (px, py) = (sx(*cx), sy(*cy));
                    if px < -6.0 || py < -6.0 || px > cw + 6.0 || py > ch + 6.0 {
                        continue;
                    }
                    ctx.begin_path();
                    let _ = ctx.arc(px, py, 4.5, 0.0, std::f64::consts::TAU);
                    ctx.fill();
                }
                for (cx, cy, hidden) in candidates {
                    let (px, py) = (sx(*cx), sy(*cy));
                    if px < -6.0 || py < -6.0 || px > cw + 6.0 || py > ch + 6.0 {
                        continue;
                    }
                    let color = if *hidden { "rgb(150, 150, 150)" } else { "rgb(110, 231, 183)" };
                    #[allow(deprecated)]
                    ctx.set_fill_style(&wasm_bindgen::JsValue::from_str(color));
                    ctx.begin_path();
                    let _ = ctx.arc(px, py, 2.5, 0.0, std::f64::consts::TAU);
                    ctx.fill();
                }

                // markers: yellow leader lines, red crosses, collision-free
                // labels (port of the desktop viewport drawing)
                if !markers.is_empty() {
                    let label_font = *label_font;
                    let line_h = label_font * 1.3 + 2.0;
                    ctx.set_font(&format!("{label_font:.0}px system-ui, sans-serif"));
                    // layout runs in screen space, sorted by x
                    let mut order: Vec<usize> = (0..markers.len())
                        .filter(|&i| {
                            markers[i].x.abs() * 2.0 <= bitmap.width() as f64
                                && markers[i].y.abs() * 2.0 <= bitmap.height() as f64
                        })
                        .collect();
                    order.sort_by(|&a, &b| sx(markers[a].x).total_cmp(&sx(markers[b].x)));
                    let xs: Vec<f32> = order.iter().map(|&i| sx(markers[i].x) as f32).collect();
                    let ys: Vec<f32> = order.iter().map(|&i| sy(markers[i].y) as f32).collect();
                    let widths: Vec<f32> = order
                        .iter()
                        .map(|&i| {
                            ctx.measure_text(&markers[i].name)
                                .map(|m| m.width() as f32)
                                .unwrap_or_else(|_| markers[i].name.len() as f32 * 7.0)
                        })
                        .collect();
                    let in_img = vec![true; order.len()];
                    let entries = layout_label_y(&xs, &ys, &widths, &in_img, line_h);

                    // leaders (yellow): cross, vertical, horizontal
                    #[allow(deprecated)]
                    ctx.set_stroke_style(&wasm_bindgen::JsValue::from_str("rgb(255, 230, 51)"));
                    ctx.set_line_width(1.0);
                    for (k, &i) in order.iter().enumerate() {
                        let (px, py) = (sx(markers[i].x), sy(markers[i].y));
                        let (lx, ly) = entries[k];
                        let label_line = py + ly as f64 - line_h as f64;
                        ctx.begin_path();
                        ctx.move_to(px - 4.0, py);
                        ctx.line_to(px + 4.0, py);
                        ctx.move_to(px, py);
                        ctx.line_to(px, label_line);
                        ctx.move_to(px, label_line);
                        ctx.line_to(px + lx as f64, label_line);
                        ctx.stroke();
                    }
                    // marker crosses (always red)
                    #[allow(deprecated)]
                    ctx.set_stroke_style(&wasm_bindgen::JsValue::from_str("rgb(255, 64, 64)"));
                    ctx.set_line_width(1.5);
                    for &i in &order {
                        let (px, py) = (sx(markers[i].x), sy(markers[i].y));
                        ctx.begin_path();
                        ctx.move_to(px - 5.0, py);
                        ctx.line_to(px + 5.0, py);
                        ctx.move_to(px, py - 5.0);
                        ctx.line_to(px, py + 5.0);
                        ctx.stroke();
                    }
                    // labels: white on dark shadow, right-aligned at leader end
                    for (k, &i) in order.iter().enumerate() {
                        if markers[i].name.is_empty() {
                            continue;
                        }
                        let (px, py) = (sx(markers[i].x), sy(markers[i].y));
                        let (lx, ly) = entries[k];
                        let label_line = py + ly as f64 - line_h as f64;
                        let tx = px + lx as f64 - widths[k] as f64;
                        let ty = label_line - 3.0;
                        #[allow(deprecated)]
                        ctx.set_fill_style(&wasm_bindgen::JsValue::from_str("rgba(0, 0, 0, 0.85)"));
                        ctx.fill_text(&markers[i].name, tx + 1.0, ty + 1.0).ok();
                        #[allow(deprecated)]
                        ctx.set_fill_style(&wasm_bindgen::JsValue::from_str("white"));
                        ctx.fill_text(&markers[i].name, tx, ty).ok();
                    }
                }
            },
        );
    }

    // screen (client) -> center-relative image coordinates
    let to_img = {
        let canvas_ref = props.canvas_ref.clone();
        let view = props.view.clone();
        let bitmap = props.bitmap.clone();
        let w = bitmap.as_ref().map(|b| b.width() as f64).unwrap_or(0.0);
        let h = bitmap.as_ref().map(|b| b.height() as f64).unwrap_or(0.0);
        move |client_x: f64, client_y: f64| -> (f64, f64) {
            let rect = canvas_ref
                .cast::<HtmlCanvasElement>()
                .map(|c| c.get_bounding_client_rect());
            let (rx, ry) = match rect {
                Some(r) => (client_x - r.left(), client_y - r.top()),
                None => (client_x, client_y),
            };
            (
                (rx - view.ox) / view.scale - w / 2.0,
                (ry - view.oy) / view.scale - h / 2.0,
            )
        }
    };

    let on_pointerdown = {
        let drag = drag.clone();
        let canvas_ref = props.canvas_ref.clone();
        let markers = props.markers.clone();
        let view = props.view.clone();
        let bitmap = props.bitmap.clone();
        Callback::from(move |e: PointerEvent| {
            e.prevent_default();
            if let Some(c) = canvas_ref.cast::<HtmlCanvasElement>() {
                let _ = c.set_pointer_capture(e.pointer_id());
            }
            let rect = canvas_ref
                .cast::<HtmlCanvasElement>()
                .map(|c| c.get_bounding_client_rect());
            let (rx, ry) = match rect {
                Some(r) => (e.client_x() as f64 - r.left(), e.client_y() as f64 - r.top()),
                None => (e.client_x() as f64, e.client_y() as f64),
            };
            // marker hit-test (screen space, 12 px radius)
            let mut mode = DragMode::Pan;
            if let Some(bmp) = &bitmap {
                let hw = bmp.width() as f64 / 2.0;
                let hh = bmp.height() as f64 / 2.0;
                for (i, m) in markers.iter().enumerate().rev() {
                    let px = (m.x + hw) * view.scale + view.ox;
                    let py = (m.y + hh) * view.scale + view.oy;
                    if (rx - px).hypot(ry - py) < 12.0 {
                        mode = DragMode::Marker(i);
                        break;
                    }
                }
            }
            let pos = (e.client_x() as f64, e.client_y() as f64);
            let mut d = drag.borrow_mut();
            d.mode = Some(mode);
            d.last = pos;
            d.start = pos;
            d.moved = false;
        })
    };

    let on_pointermove = {
        let drag = drag.clone();
        let pan_cb = props.callbacks.pan.clone();
        let marker_drag = props.callbacks.marker_drag.clone();
        let hover = props.callbacks.hover.clone();
        let to_img = to_img.clone();
        let scale = props.view.scale;
        Callback::from(move |e: PointerEvent| {
            let mut d = drag.borrow_mut();
            let mode = match &d.mode {
                Some(m) => m.clone(),
                None => {
                    // free hover: report position for peak identification
                    let (ix, iy) = to_img(e.client_x() as f64, e.client_y() as f64);
                    hover.emit((ix, iy));
                    return;
                }
            };
            let pos = (e.client_x() as f64, e.client_y() as f64);
            let dx = pos.0 - d.last.0;
            let dy = pos.1 - d.last.1;
            d.last = pos;
            if (pos.0 - d.start.0).hypot(pos.1 - d.start.1) > 3.0 {
                d.moved = true;
            }
            if !d.moved {
                return;
            }
            match mode {
                DragMode::Pan => pan_cb.emit((dx, dy)),
                DragMode::Marker(i) => marker_drag.emit((i, dx / scale, dy / scale)),
            }
        })
    };

    let on_pointerleave = {
        let hover_end = props.callbacks.hover_end.clone();
        Callback::from(move |_: PointerEvent| hover_end.emit(()))
    };

    let on_pointerup = {
        let drag = drag.clone();
        let canvas_ref = props.canvas_ref.clone();
        let click = props.callbacks.click.clone();
        let drag_end = props.callbacks.drag_end.clone();
        let to_img = to_img.clone();
        Callback::from(move |e: PointerEvent| {
            let mode = drag.borrow_mut().mode.take();
            if let Some(c) = canvas_ref.cast::<HtmlCanvasElement>() {
                let _ = c.release_pointer_capture(e.pointer_id());
            }
            let moved = drag.borrow().moved;
            match mode {
                Some(DragMode::Pan) if !moved => {
                    let (ix, iy) = to_img(e.client_x() as f64, e.client_y() as f64);
                    click.emit((ix, iy));
                }
                Some(DragMode::Marker(_)) if moved => drag_end.emit(()),
                _ => {}
            }
        })
    };

    let on_pointercancel = {
        let drag = drag.clone();
        Callback::from(move |_: PointerEvent| {
            drag.borrow_mut().mode = None;
        })
    };

    let on_contextmenu = {
        let context = props.callbacks.context.clone();
        let to_img = to_img.clone();
        Callback::from(move |e: MouseEvent| {
            e.prevent_default();
            let (ix, iy) = to_img(e.client_x() as f64, e.client_y() as f64);
            context.emit((ix, iy));
        })
    };

    let on_wheel = {
        let zoom = props.callbacks.zoom.clone();
        let canvas_ref = props.canvas_ref.clone();
        Callback::from(move |e: WheelEvent| {
            e.prevent_default();
            let cx = e.client_x() as f64
                - canvas_ref
                    .cast::<HtmlCanvasElement>()
                    .map(|c| c.get_bounding_client_rect().left())
                    .unwrap_or(0.0);
            let cy = e.client_y() as f64
                - canvas_ref
                    .cast::<HtmlCanvasElement>()
                    .map(|c| c.get_bounding_client_rect().top())
                    .unwrap_or(0.0);
            let factor = 1.0015f64.powf(-(e.delta_y() as f64));
            zoom.emit((cx, cy, factor));
        })
    };

    html! {
        <canvas
            ref={props.canvas_ref.clone()}
            class="viewport"
            onpointerdown={on_pointerdown}
            onpointermove={on_pointermove}
            onpointerup={on_pointerup}
            onpointercancel={on_pointercancel}
            onpointerleave={on_pointerleave}
            oncontextmenu={on_contextmenu}
            onwheel={on_wheel}
        />
    }
}

/// Port of the desktop `viewport::layout_label_y`: place labels right of
/// their crosses, pushing colliding ones up; iterate to a fixed point
/// (the C single pass can leave overlaps). Inputs must be sorted by x.
fn layout_label_y(
    xs: &[f32],
    ys: &[f32],
    widths: &[f32],
    in_img: &[bool],
    line_h: f32,
) -> Vec<(f32, f32)> {
    let n = xs.len();
    let mut label_x = vec![0.0f32; n];
    let mut label_y = vec![0.0f32; n];
    let cross = 4.0f32;
    let overlap = |a1: f32, l1: f32, a2: f32, l2: f32| a1 <= a2 + l2 && a1 + l1 >= a2;

    for i in 0..n {
        if !in_img[i] {
            continue;
        }
        label_x[i] = widths[i] + 1.0;
        label_y[i] = 0.0;

        let mut colliding: Vec<usize> = Vec::new();
        for j in (0..i).rev() {
            if !in_img[j] {
                continue;
            }
            if overlap(xs[i], label_x[i], xs[j] - cross, label_x[j]) {
                colliding.push(j);
            } else {
                break;
            }
        }
        colliding.sort_by(|&a, &b| {
            let ya = ys[a] + label_y[a];
            let yb = ys[b] + label_y[b];
            yb.total_cmp(&ya)
        });
        for _round in 0..64 {
            let mut pushed = false;
            for &j in &colliding {
                if overlap(
                    ys[i] + label_y[i] - line_h,
                    line_h,
                    ys[j] + label_y[j] - line_h,
                    line_h,
                ) || overlap(ys[i] + label_y[i] - line_h, line_h, ys[j] - cross, 2.0 * cross)
                {
                    let ny = (ys[j] + label_y[j] - ys[i] - line_h - 2.0).round();
                    if ny < label_y[i] {
                        label_y[i] = ny;
                        pushed = true;
                    }
                }
            }
            if !pushed {
                break;
            }
        }
    }

    (0..n).map(|i| (label_x[i], label_y[i])).collect()
}
