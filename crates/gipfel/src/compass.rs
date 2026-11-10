//! Compass strip (canvas port of the desktop `compass.rs`, Aligned style):
//! shares the image's horizontal transform so each direction sits above
//! its position in the image; drag horizontally to change the view azimuth.

use web_sys::{HtmlCanvasElement, PointerEvent};use yew::prelude::*;

use gipfel_core::projection::{projection_from_type, ProjArgs, Projection, ProjectionType, ViewParams};

use crate::bridge;

pub const STRIP_H: f64 = 56.0;

/// Damped, clamped Newton iteration for a scalar inverse projection: find
/// `a` such that `f(a) ≈ target` (identical to the desktop solver).
pub fn newton_inverse(
    target: f64,
    mut f: impl FnMut(f64) -> Option<f64>,
    seed: f64,
    lo: f64,
    hi: f64,
) -> f64 {
    let mut a = seed.clamp(lo, hi);
    for _ in 0..6 {
        let Some(y) = f(a) else { break };
        let resid = y - target;
        let h = 1e-5;
        let (Some(y1), Some(y2)) = (f(a + h), f(a - h)) else {
            break;
        };
        let d = (y1 - y2) / (2.0 * h);
        if !d.is_finite() || d.abs() < 1e-9 {
            break;
        }
        let step = (resid / d).clamp(-0.3, 0.3);
        a = (a - step).clamp(lo, hi);
        if step.abs() < 1e-10 {
            break;
        }
    }
    a
}

/// azimuth whose image x is `x_img`, measured with `c_ref` as center angle
pub fn alph_at_img_x(
    ptype: ProjectionType,
    proj: &dyn Projection,
    p: &ViewParams,
    x_img: f64,
    c_ref: f64,
) -> f64 {
    let mut p = *p;
    p.a_center = c_ref;
    let seed = match ptype {
        ProjectionType::Rectilinear => c_ref + (x_img / p.scale).atan(),
        ProjectionType::Cylindrical => c_ref + x_img / p.scale,
    };
    let (lo, hi) = match ptype {
        ProjectionType::Rectilinear => (c_ref - 1.55, c_ref + 1.55),
        ProjectionType::Cylindrical => (c_ref - 3.1, c_ref + 3.1),
    };
    newton_inverse(
        x_img,
        |a| Some(proj.mac_x(ProjArgs::from_params(&p, a, 0.0))),
        seed,
        lo,
        hi,
    )
}

const DIRS: [(f64, &str); 8] = [
    (0.0, "N"),
    (45.0, "NE"),
    (90.0, "E"),
    (135.0, "SE"),
    (180.0, "S"),
    (225.0, "SW"),
    (270.0, "W"),
    (315.0, "NW"),
];

/// everything the strip needs; all plain data so Yew can diff it
#[derive(Clone, Copy, PartialEq)]
pub struct CompassData {
    pub ptype: ProjectionType,
    pub parms: ViewParams,
    pub center_deg: f64,
    pub zoom: f64,
    /// screen x of the image center (strip shares the image transform)
    pub cx: f64,
    /// lens FOV half-angle limit, radians (0 = none)
    pub max_view_angle: f64,
}

#[derive(Properties, PartialEq)]
pub struct Props {
    pub data: CompassData,
    /// bumped on window resize to force a redraw
    pub tick: u32,
    /// new azimuth in degrees while dragging
    pub on_change: Callback<f64>,
}

#[function_component(Compass)]
pub fn compass(props: &Props) -> Html {
    let canvas_ref = use_node_ref();
    let drag = use_mut_ref(|| Option::<(f64, f64)>::None); // (press x, center_deg)

    let d = props.data;

    // draw
    {
        let canvas_ref = canvas_ref.clone();
        use_effect_with((d, props.tick), move |(d, _tick)| {
            let Some(canvas) = canvas_ref.cast::<HtmlCanvasElement>() else {
                return;
            };
            let Some(ctx) = bridge::ctx2d(&canvas) else {
                return;
            };
            let dpr = bridge::window().device_pixel_ratio();
            let w = canvas.client_width() as f64;
            let bw = (w * dpr).max(1.0) as u32;
            let bh = (STRIP_H * dpr).max(1.0) as u32;
            if canvas.width() != bw || canvas.height() != bh {
                canvas.set_width(bw);
                canvas.set_height(bh);
            }
            let _ = ctx.set_transform(dpr, 0.0, 0.0, dpr, 0.0, 0.0);

            // theme colors come from the CSS variables, so the strip
            // follows light/dark preference like the rest of the UI
            let on = bridge::css_var("--compass-fg", "rgb(217, 217, 217)");
            let bg = bridge::css_var("--compass-bg", "rgb(41, 41, 41)");
            let divider = bridge::css_var("--compass-divider", "rgb(82, 82, 82)");
            let red = "rgb(255, 102, 64)";

            let fill = |c: &str| {
                #[allow(deprecated)]
                ctx.set_fill_style(&wasm_bindgen::JsValue::from_str(c));
            };
            let stroke = |c: &str, width: f64| {
                #[allow(deprecated)]
                ctx.set_stroke_style(&wasm_bindgen::JsValue::from_str(c));
                ctx.set_line_width(width);
            };

            fill(&bg);
            ctx.fill_rect(0.0, 0.0, w, STRIP_H);
            stroke(&divider, 1.0);
            ctx.begin_path();
            ctx.move_to(0.0, 0.5);
            ctx.line_to(w, 0.5);
            ctx.stroke();

            let label_y = 17.0f64; // text baseline of the direction row
            let y0 = (STRIP_H - 10.0).round(); // baseline of the ticks
            let tri_y = 25.0f64;
            let z = d.zoom;
            let proj = projection_from_type(d.ptype);
            let cx = d.cx;
            let c = d.parms.a_center;

            let x_img_at = |alph: f64| proj.get_coordinates(alph, 0.0, &d.parms).0;

            // adaptive tick step: first that yields >= 6 px spacing
            let ppd = ((x_img_at(c + 1f64.to_radians()) - x_img_at(c - 1f64.to_radians()))
                * z
                / 2.0)
                .abs();
            let step = [1.0f64, 2.0, 5.0, 10.0, 15.0, 30.0, 45.0, 90.0]
                .into_iter()
                .find(|s| s * ppd >= 6.0)
                .unwrap_or(90.0);

            let a_lo = alph_at_img_x(d.ptype, &*proj, &d.parms, -cx / z, c)
                .min(c - std::f64::consts::PI);
            let a_hi = alph_at_img_x(d.ptype, &*proj, &d.parms, (w - cx) / z, c)
                .max(c + std::f64::consts::PI);
            // rectilinear folds directions behind the camera onto the same
            // pixels; also clamp to the lens FOV (distortion can fold
            // out-of-view directions back onto the strip)
            let mut fold = match d.ptype {
                ProjectionType::Rectilinear => 1.535,
                ProjectionType::Cylindrical => std::f64::consts::PI,
            };
            if d.max_view_angle > 0.0 {
                fold = fold.min(d.max_view_angle);
            }
            let a_lo = a_lo.max(c - fold);
            let a_hi = a_hi.min(c + fold);

            // baseline
            stroke(&on, 1.0);
            ctx.begin_path();
            ctx.move_to(0.0, y0 + 0.5);
            ctx.line_to(w, y0 + 0.5);
            ctx.stroke();

            // ticks
            let step_rad = step.to_radians();
            let mut a = (a_lo / step_rad).floor() * step_rad;
            while a <= a_hi {
                let x = cx + x_img_at(a) * z;
                if x.is_finite() && x >= -1.0 && x <= w + 1.0 {
                    let rem = a.to_degrees().rem_euclid(45.0);
                    let major = rem.min(45.0 - rem) < step / 2.0;
                    let x = x.round();
                    ctx.begin_path();
                    ctx.move_to(x, y0);
                    ctx.line_to(x, y0 - if major { 12.0 } else { 6.0 });
                    ctx.stroke();
                }
                a += step_rad;
            }

            // direction labels
            ctx.set_font("13px system-ui, sans-serif");
            ctx.set_text_align("center");
            for (deg, name) in DIRS {
                let alph = deg.to_radians();
                let rel = (alph - c + std::f64::consts::PI).rem_euclid(std::f64::consts::TAU)
                    - std::f64::consts::PI;
                if rel.abs() > fold {
                    continue;
                }
                let x = cx + x_img_at(alph) * z;
                if !x.is_finite() || x < -20.0 || x > w + 20.0 {
                    continue;
                }
                fill(if deg == 0.0 { red } else { &on });
                ctx.fill_text(name, x, label_y).ok();
            }
            ctx.set_text_align("start");

            // center marker: triangle + hairline + azimuth readout
            fill(red);
            ctx.begin_path();
            ctx.move_to(cx - 6.0, tri_y);
            ctx.line_to(cx + 6.0, tri_y);
            ctx.line_to(cx, tri_y + 8.0);
            ctx.close_path();
            ctx.fill();
            stroke(red, 1.0);
            ctx.begin_path();
            ctx.move_to(cx.round(), tri_y + 8.0);
            ctx.line_to(cx.round(), y0);
            ctx.stroke();
            fill(red);
            ctx.fill_text(&format!("{:05.1}°", d.center_deg.rem_euclid(360.0)), cx + 10.0, tri_y + 9.0)
                .ok();
        });
    }

    let local_x = {
        let canvas_ref = canvas_ref.clone();
        move |client_x: f64| {
            client_x
                - canvas_ref
                    .cast::<HtmlCanvasElement>()
                    .map(|c| c.get_bounding_client_rect().left())
                    .unwrap_or(0.0)
        }
    };

    let on_pointerdown = {
        let drag = drag.clone();
        let canvas_ref = canvas_ref.clone();
        let center_deg = props.data.center_deg;
        Callback::from(move |e: PointerEvent| {
            e.prevent_default();
            if let Some(c) = canvas_ref.cast::<HtmlCanvasElement>() {
                let _ = c.set_pointer_capture(e.pointer_id());
            }
            *drag.borrow_mut() = Some((e.client_x() as f64, center_deg));
        })
    };
    let on_pointermove = {
        let drag = drag.clone();
        let on_change = props.on_change.clone();
        let d = props.data;
        Callback::from(move |e: PointerEvent| {
            let Some((x0, c0_deg)) = *drag.borrow() else {
                return;
            };
            let proj = projection_from_type(d.ptype);
            let c0 = c0_deg.to_radians();
            let lx0 = local_x(e.client_x() as f64);
            let a0 = alph_at_img_x(d.ptype, &*proj, &d.parms, (x0 - d.cx) / d.zoom, c0);
            let a1 = alph_at_img_x(d.ptype, &*proj, &d.parms, (lx0 - d.cx) / d.zoom, c0);
            on_change.emit((c0 - (a1 - a0)).to_degrees());
        })
    };
    let on_pointerup = {
        let drag = drag.clone();
        let canvas_ref = canvas_ref.clone();
        Callback::from(move |e: PointerEvent| {
            *drag.borrow_mut() = None;
            if let Some(c) = canvas_ref.cast::<HtmlCanvasElement>() {
                let _ = c.release_pointer_capture(e.pointer_id());
            }
        })
    };

    html! {
        <canvas
            ref={canvas_ref}
            class="compass"
            style={format!("height: {}px", STRIP_H)}
            onpointerdown={on_pointerdown}
            onpointermove={on_pointermove}
            onpointerup={on_pointerup.clone()}
            onpointercancel={on_pointerup}
        />
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn roundtrip(ptype: ProjectionType, k0: f64, k1: f64, x0: f64) {
        let proj = projection_from_type(ptype);
        let mut parms = ViewParams::new();
        parms.a_center = 0.3;
        parms.scale = 3000.0;
        parms.k0 = k0;
        parms.k1 = k1;
        parms.x0 = x0;
        for x in [-2500.0, -800.0, -50.0, 0.0, 120.0, 900.0, 2400.0] {
            let a = alph_at_img_x(ptype, &*proj, &parms, x, parms.a_center);
            let (x2, _) = proj.get_coordinates(a, 0.0, &parms);
            assert!(
                (x2 - x).abs() < 1e-6,
                "{ptype:?} x={x}: round-trips to {x2} (alph {a})"
            );
        }
    }

    #[test]
    fn inverse_rectilinear() {
        roundtrip(ProjectionType::Rectilinear, 0.0, 0.0, 0.0);
    }
    #[test]
    fn inverse_rectilinear_distorted() {
        roundtrip(ProjectionType::Rectilinear, 0.002, -0.0003, 0.01);
    }
    #[test]
    fn inverse_cylindrical() {
        roundtrip(ProjectionType::Cylindrical, 0.0, 0.0, 0.0);
    }
}
