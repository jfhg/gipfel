//! Viewport canvas: browser-native bitmap blitted with a pan/zoom
//! transform. Overlay drawing (markers, labels, compass) is added in W3.

use web_sys::{HtmlCanvasElement, ImageBitmap, PointerEvent, WheelEvent};use yew::prelude::*;

use crate::bridge;

/// Image -> screen transform: `screen = img * scale + (ox, oy)`.
/// Same convention as the desktop viewport widget.
#[derive(Clone, Debug, PartialEq, Default)]
pub struct ViewState {
    pub scale: f64,
    pub ox: f64,
    pub oy: f64,
}

#[derive(Clone, PartialEq)]
pub struct ViewCallbacks {
    pub pan: Callback<(f64, f64)>,
    /// (cursor x, cursor y, factor) — zoom anchored at the cursor.
    pub zoom: Callback<(f64, f64, f64)>,
}

#[derive(Properties, PartialEq)]
pub struct Props {
    pub bitmap: Option<ImageBitmap>,
    pub view: ViewState,
    pub callbacks: ViewCallbacks,
    pub canvas_ref: NodeRef,
    /// bumped on window resize to force a redraw
    pub tick: u32,
}

#[function_component(CanvasView)]
pub fn canvas_view(props: &Props) -> Html {
    let dragging = use_mut_ref(|| false);
    let last = use_mut_ref(|| (0.0f64, 0.0f64));

    // redraw on every view/bitmap change and on resize ticks
    {
        let bitmap = props.bitmap.clone();
        let view = props.view.clone();
        let canvas_ref = props.canvas_ref.clone();
        use_effect_with((bitmap, view, props.tick), move |(bitmap, view, _tick)| {
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
            if let Some(bitmap) = bitmap {
                let _ = ctx.set_transform(
                    dpr * view.scale,
                    0.0,
                    0.0,
                    dpr * view.scale,
                    dpr * view.ox,
                    dpr * view.oy,
                );
                let _ = ctx.draw_image_with_image_bitmap(bitmap, 0.0, 0.0);
            }
        });
    }

    let on_pointerdown = {
        let dragging = dragging.clone();
        let last = last.clone();
        let canvas_ref = props.canvas_ref.clone();
        Callback::from(move |e: PointerEvent| {
            e.prevent_default();
            if let Some(c) = canvas_ref.cast::<HtmlCanvasElement>() {
                let _ = c.set_pointer_capture(e.pointer_id());
            }
            *dragging.borrow_mut() = true;
            *last.borrow_mut() = (e.client_x() as f64, e.client_y() as f64);
        })
    };
    let on_pointermove = {
        let dragging = dragging.clone();
        let last = last.clone();
        let pan = props.callbacks.pan.clone();
        Callback::from(move |e: PointerEvent| {
            if !*dragging.borrow() {
                return;
            }
            let cur = (e.client_x() as f64, e.client_y() as f64);
            let prev = *last.borrow();
            *last.borrow_mut() = cur;
            pan.emit((cur.0 - prev.0, cur.1 - prev.1));
        })
    };
    let on_pointerup = {
        let dragging = dragging.clone();
        let canvas_ref = props.canvas_ref.clone();
        Callback::from(move |e: PointerEvent| {
            *dragging.borrow_mut() = false;
            if let Some(c) = canvas_ref.cast::<HtmlCanvasElement>() {
                let _ = c.release_pointer_capture(e.pointer_id());
            }
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
            onpointerup={on_pointerup.clone()}
            onpointercancel={on_pointerup}
            onwheel={on_wheel}
        />
    }
}
