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
    /// projected positions of all visible hills (small guide dots)
    pub candidates: Vec<(f64, f64)>,
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
        use_effect_with(
            (bitmap, view, props.tick, markers.clone(), candidates.clone()),
            move |(bitmap, view, _tick, markers, candidates)| {
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

                // guide dots for all visible peaks
                #[allow(deprecated)]
                ctx.set_fill_style(&wasm_bindgen::JsValue::from_str("rgba(120, 220, 160, 0.8)"));
                for (cx, cy) in candidates {
                    let (px, py) = (sx(*cx), sy(*cy));
                    if px < -4.0 || py < -4.0 || px > cw + 4.0 || py > ch + 4.0 {
                        continue;
                    }
                    ctx.fill_rect(px - 1.5, py - 1.5, 3.0, 3.0);
                }

                // markers: red cross + name
                ctx.set_font("12px system-ui, sans-serif");
                for m in markers {
                    let (px, py) = (sx(m.x), sy(m.y));
                    #[allow(deprecated)]
                    ctx.set_stroke_style(&wasm_bindgen::JsValue::from_str("rgb(255, 64, 64)"));
                    ctx.set_line_width(1.5);
                    ctx.begin_path();
                    ctx.move_to(px - 5.0, py);
                    ctx.line_to(px + 5.0, py);
                    ctx.move_to(px, py - 5.0);
                    ctx.line_to(px, py + 5.0);
                    ctx.stroke();
                    if !m.name.is_empty() {
                        // dark shadow + white text stays readable on any photo
                        #[allow(deprecated)]
                        ctx.set_fill_style(&wasm_bindgen::JsValue::from_str("rgba(0, 0, 0, 0.8)"));
                        ctx.fill_text(&m.name, px + 7.0, py - 5.0).ok();
                        #[allow(deprecated)]
                        ctx.set_fill_style(&wasm_bindgen::JsValue::from_str("white"));
                        ctx.fill_text(&m.name, px + 6.0, py - 6.0).ok();
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
        let pan = props.callbacks.pan.clone();
        let marker_drag = props.callbacks.marker_drag.clone();
        let scale = props.view.scale;
        Callback::from(move |e: PointerEvent| {
            let mut d = drag.borrow_mut();
            let mode = match &d.mode {
                Some(m) => m.clone(),
                None => return,
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
                DragMode::Pan => pan.emit((dx, dy)),
                DragMode::Marker(i) => marker_drag.emit((i, dx / scale, dy / scale)),
            }
        })
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
            oncontextmenu={on_contextmenu}
            onwheel={on_wheel}
        />
    }
}
