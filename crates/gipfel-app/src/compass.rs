//! Experimental compass strip: shows N/E/S/W at their projected positions
//! relative to the view direction; drag horizontally to change the direction.
//!
//! Two experimental styles (switchable from the View menu):
//! - `Aligned`: the strip shares the image's horizontal transform, so each
//!   direction appears exactly above its position in the image. Grab-drag
//!   keeps the direction under the cursor fixed.
//! - `Ruler`: a fixed 0..360° ruler across the widget width; press or drag
//!   sets the direction to the value under the cursor. The shaded band marks
//!   the current horizontal field of view.
//!
//! Layout (top to bottom): direction labels, center marker, tick scale on a
//! baseline. The whole strip sits on a secondary-container background so it
//! is visually distinct from the image (pan) area.

use std::cell::RefCell;
use std::rc::Rc;

use cosmic::iced::advanced::graphics::geometry::Renderer as _;
use cosmic::iced::advanced::renderer::Style as RendererStyle;
use cosmic::iced::advanced::text::{self as adv_text, paragraph::Plain, LineHeight};
use cosmic::iced::advanced::widget::{tree, Tree, Widget as WidgetTrait};
use cosmic::iced::advanced::{Clipboard, Layout, Shell};
use cosmic::iced::mouse::{self, Button, Cursor, Interaction};
use cosmic::iced::widget::canvas::{Frame, Path, Stroke};
use cosmic::iced::advanced::graphics::geometry::Style as GeoStyle;
use cosmic::iced::{alignment, Color, Event, Length, Pixels, Point, Rectangle, Size};
use cosmic::Renderer as R;
use cosmic::Theme;

use gipfel_core::projection::{
    projection_from_type, Projection, ProjArgs, ProjectionType, ViewParams,
};

pub const STRIP_H: f32 = 56.0;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CompassStyle {
    Off,
    Aligned,
    Ruler,
}

/// inverse: azimuth whose image x is `x_img`, measured with `c_ref` as
/// center angle. Analytic base inverse + damped Newton refinement so lens
/// distortion is honoured.
/// Damped, clamped Newton iteration for a scalar inverse projection: find
/// `a` such that `f(a) ≈ target`. `f` returns `None` where the forward
/// projection is undefined (the iteration stops there). The step is clamped
/// to ±0.3 rad and `a` kept in `[lo, hi]` to stay off the projection poles.
pub(crate) fn newton_inverse(
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

pub fn alph_at_img_x(
    ptype: ProjectionType,
    proj: &dyn Projection,
    parms: &ViewParams,
    x_img: f64,
    c_ref: f64,
) -> f64 {
    let mut p = *parms;
    p.a_center = c_ref;
    // analytic base inverse (ignores distortion); Newton refines it
    let seed = match ptype {
        ProjectionType::Rectilinear => c_ref + (x_img / p.scale).atan(),
        ProjectionType::Cylindrical => c_ref + x_img / p.scale,
    };
    // rectilinear has a pole at |alph - center| = 90°; keep Newton inside
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

pub struct Compass<M: Clone + Send + 'static> {
    pub style: CompassStyle,
    pub center_deg: f64,
    pub zoom: f32,
    pub pan_x: f32,
    pub parms: ViewParams,
    pub ptype: ProjectionType,
    /// horizontal field of view of the image, degrees (Ruler shading)
    pub fov_deg: f64,
    /// lens field-of-view half-angle limit in radians (0 = none). Ticks and
    /// direction labels beyond this are dropped: a distortion model can fold
    /// out-of-view directions back onto the strip, showing bogus ticks.
    pub max_view_angle: f64,
    pub on_change: Rc<dyn Fn(f64) -> M>,
}

impl<M: Clone + Send + 'static> Clone for Compass<M> {
    fn clone(&self) -> Self {
        Compass {
            style: self.style,
            center_deg: self.center_deg,
            zoom: self.zoom,
            pan_x: self.pan_x,
            parms: self.parms,
            ptype: self.ptype,
            fov_deg: self.fov_deg,
            max_view_angle: self.max_view_angle,
            on_change: self.on_change.clone(),
        }
    }
}

#[derive(Default)]
struct CompassState {
    /// (press x, center angle at press)
    drag: Option<(f32, f64)>,
    /// strong refs: the text pipeline only keeps a Weak to each paragraph
    paragraphs: RefCell<Vec<Plain<<R as adv_text::Renderer>::Paragraph>>>,
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

/// one text to draw: content, x, y, center around x?, color
struct Txt {
    text: String,
    x: f32,
    y: f32,
    centered: bool,
    color: Color,
}

fn stroke_tick(frame: &mut Frame<R>, x: f32, y0: f32, h: f32, w: f32, color: Color) {
    if !x.is_finite() {
        return;
    }
    // snap thin lines to integer coords for crisp 1 px strokes
    let x = if w < 1.5 { x.round() } else { x };
    let p = Path::new(|p| {
        p.move_to(Point::new(x, y0));
        p.line_to(Point::new(x, y0 - h));
    });
    frame.stroke(
        &p,
        Stroke {
            width: w,
            style: GeoStyle::Solid(color),
            ..Default::default()
        },
    );
}

impl<M: Clone + Send + 'static> Compass<M> {
    /// image-space x of azimuth `alph` (on the horizon), current parms
    fn x_img_at(&self, proj: &dyn Projection, alph: f64) -> f64 {
        proj.get_coordinates(alph, 0.0, &self.parms).0
    }

    fn alph_at_img_x(
        &self,
        proj: &dyn Projection,
        x_img: f64,
        c_ref: f64,
    ) -> f64 {
        alph_at_img_x(self.ptype, proj, &self.parms, x_img, c_ref)
    }

    /// screen x of the image center for the Aligned style
    fn center_x(&self, b: Rectangle) -> f32 {
        b.x + b.width / 2.0 + self.pan_x
    }
}

impl<M: Clone + Send + 'static> WidgetTrait<M, Theme, R> for Compass<M> {
    fn size(&self) -> Size<Length> {
        Size::new(Length::Fill, Length::Fixed(STRIP_H))
    }

    fn tag(&self) -> tree::Tag {
        tree::Tag::of::<CompassState>()
    }

    fn state(&self) -> tree::State {
        tree::State::new(CompassState::default())
    }

    fn layout(
        &mut self,
        _tree: &mut Tree,
        _renderer: &R,
        limits: &cosmic::iced::advanced::layout::Limits,
    ) -> cosmic::iced::advanced::layout::Node {
        cosmic::iced::advanced::layout::Node::new(Size::new(limits.max().width, STRIP_H))
    }

    fn draw(
        &self,
        tree: &Tree,
        renderer: &mut R,
        theme: &Theme,
        _style: &RendererStyle,
        layout: Layout<'_>,
        _cursor: Cursor,
        _viewport: &Rectangle,
    ) {
        use cosmic::iced::advanced::text::Renderer as _;

        let b = layout.bounds();
        let _ = theme;
        // dark instrument panel, clearly distinct from the image area
        let on: Color = Color::from_rgb(0.85, 0.85, 0.85);
        let bg: Color = Color::from_rgb(0.16, 0.16, 0.16);
        let divider: Color = Color::from_rgb(0.32, 0.32, 0.32);
        let red = Color::from_rgb(1.0, 0.4, 0.25);

        // geometry rows (snapped so lines stay crisp)
        let label_y = (b.y + 5.0).round();
        let y0 = (b.y + STRIP_H - 10.0).round(); // baseline
        let tri_y = (b.y + 25.0).round();
        let z = self.zoom;
        let proj = projection_from_type(self.ptype);

        let mut texts: Vec<Txt> = Vec::new();
        let mut frame = Frame::with_bounds(renderer, b);

        // panel background + top divider: clearly a separate, draggable area
        let panel = Path::new(|p| p.rectangle(b.position(), b.size()));
        frame.fill(&panel, bg);
        let sep = Path::new(|p| {
            p.move_to(Point::new(b.x, b.y + 0.5));
            p.line_to(Point::new(b.x + b.width, b.y + 0.5));
        });
        frame.stroke(
            &sep,
            Stroke {
                width: 1.0,
                style: GeoStyle::Solid(divider),
                ..Default::default()
            },
        );

        match self.style {
            CompassStyle::Off => return,
            CompassStyle::Aligned => {
                let cx = self.center_x(b);
                let c = self.parms.a_center;
                // pixels per degree at the center
                let ppd = ((self.x_img_at(&*proj, c + 1f64.to_radians())
                    - self.x_img_at(&*proj, c - 1f64.to_radians()))
                    * z as f64
                    / 2.0)
                    .abs();
                let step = [1.0f64, 2.0, 5.0, 10.0, 15.0, 30.0, 45.0, 90.0]
                    .into_iter()
                    .find(|s| s * ppd >= 6.0)
                    .unwrap_or(90.0);

                let a_lo = self
                    .alph_at_img_x(&*proj, ((b.x - cx) / z) as f64, c)
                    .min(c - std::f64::consts::PI);
                let a_hi = self
                    .alph_at_img_x(&*proj, ((b.x + b.width - cx) / z) as f64, c)
                    .max(c + std::f64::consts::PI);
                // the rectilinear projection folds directions behind the
                // camera onto the same pixels (tan has 180 deg period);
                // never draw beyond +-88 deg of the center. Cylindrical is
                // linear in azimuth and may legitimately span > 180 deg.
                let fold = match self.ptype {
                    ProjectionType::Rectilinear => 1.535,
                    ProjectionType::Cylindrical => std::f64::consts::PI,
                };
                // additionally clamp to the lens FOV so distortion-folded
                // out-of-view directions do not get ticks/labels
                let fold = if self.max_view_angle > 0.0 {
                    fold.min(self.max_view_angle)
                } else {
                    fold
                };
                let a_lo = a_lo.max(c - fold);
                let a_hi = a_hi.min(c + fold);

                // baseline
                let line = Path::new(|p| {
                    p.move_to(Point::new(b.x, y0 + 0.5));
                    p.line_to(Point::new(b.x + b.width, y0 + 0.5));
                });
                frame.stroke(
                    &line,
                    Stroke {
                        width: 1.0,
                        style: GeoStyle::Solid(on),
                        ..Default::default()
                    },
                );

                // ticks (rise from the baseline, stay below the labels);
                // `a` is radians, `step` degrees -> convert once
                let step_rad = step.to_radians();
                let mut a = (a_lo / step_rad).floor() * step_rad;
                while a <= a_hi {
                    let x = cx + self.x_img_at(&*proj, a) as f32 * z;
                    let rem = a.to_degrees().rem_euclid(45.0);
                    let major = rem.min(45.0 - rem) < step / 2.0;
                    stroke_tick(&mut frame, x, y0, if major { 12.0 } else { 6.0 }, 1.0, on);
                    a += step_rad;
                }

                // direction labels on the top row; skip directions outside
                // the +-88 deg window (they would fold onto the same pixel)
                for (deg, name) in DIRS {
                    let alph = deg.to_radians();
                    let rel = (alph - c + std::f64::consts::PI).rem_euclid(std::f64::consts::TAU)
                        - std::f64::consts::PI;
                    if rel.abs() > fold {
                        continue;
                    }
                    let x = cx + self.x_img_at(&*proj, alph) as f32 * z;
                    if !x.is_finite() || x < b.x - 20.0 || x > b.x + b.width + 20.0 {
                        continue;
                    }
                    let color = if deg == 0.0 { red } else { on };
                    texts.push(Txt {
                        text: name.to_string(),
                        x,
                        y: label_y,
                        centered: true,
                        color,
                    });
                }

                // center marker: triangle + hairline (tick area only)
                let tri = Path::new(|p| {
                    p.move_to(Point::new(cx - 6.0, tri_y));
                    p.line_to(Point::new(cx + 6.0, tri_y));
                    p.line_to(Point::new(cx, tri_y + 8.0));
                    p.close();
                });
                frame.fill(&tri, red);
                let hair = Path::new(|p| {
                    p.move_to(Point::new(cx.round(), tri_y + 8.0));
                    p.line_to(Point::new(cx.round(), y0));
                });
                frame.stroke(
                    &hair,
                    Stroke {
                        width: 1.0,
                        style: GeoStyle::Solid(red),
                        ..Default::default()
                    },
                );

                let deg = self.center_deg.rem_euclid(360.0);
                texts.push(Txt {
                    text: format!("{deg:05.1}°"),
                    x: cx + 10.0,
                    y: tri_y - 3.0,
                    centered: false,
                    color: red,
                });
            }
            CompassStyle::Ruler => {
                let bx = b.x;
                let f = |alph_deg: f64| {
                    bx + (alph_deg.rem_euclid(360.0) / 360.0) as f32 * b.width
                };
                let center = self.center_deg.rem_euclid(360.0);
                let mx = f(center);

                // FOV band (between label row and baseline)
                let half = (self.fov_deg / 2.0 / 360.0) as f32 * b.width;
                let x0 = (mx - half).max(bx);
                let x1 = (mx + half).min(bx + b.width);
                if x1 > x0 {
                    let band = Path::new(|p| {
                        p.rectangle(
                            Point::new(x0, tri_y),
                            Size::new(x1 - x0, y0 - tri_y),
                        );
                    });
                    frame.fill(&band, Color::from_rgba(0.4, 0.65, 1.0, 0.20));
                }

                // baseline + ticks every 5 deg
                let line = Path::new(|p| {
                    p.move_to(Point::new(bx, y0 + 0.5));
                    p.line_to(Point::new(bx + b.width, y0 + 0.5));
                });
                frame.stroke(
                    &line,
                    Stroke {
                        width: 1.0,
                        style: GeoStyle::Solid(on),
                        ..Default::default()
                    },
                );
                for deg in (0..360).step_by(5) {
                    let major = deg % 45 == 0;
                    stroke_tick(
                        &mut frame,
                        f(deg as f64),
                        y0,
                        if major { 12.0 } else { 6.0 },
                        1.0,
                        on,
                    );
                }
                for (deg, name) in DIRS {
                    let color = if deg == 0.0 { red } else { on };
                    texts.push(Txt {
                        text: name.to_string(),
                        x: f(deg),
                        y: label_y,
                        centered: true,
                        color,
                    });
                }

                // marker
                let tri = Path::new(|p| {
                    p.move_to(Point::new(mx - 6.0, tri_y));
                    p.line_to(Point::new(mx + 6.0, tri_y));
                    p.line_to(Point::new(mx, tri_y + 8.0));
                    p.close();
                });
                frame.fill(&tri, red);
                texts.push(Txt {
                    text: format!("{center:05.1}°"),
                    x: mx + 10.0,
                    y: tri_y - 3.0,
                    centered: false,
                    color: red,
                });
            }
        }

        renderer.draw_geometry(frame.into_geometry());

        // text (kept alive in state until next frame; positions rounded
        // so glyphs land on pixel boundaries)
        let font = renderer.default_font();
        let state = tree.state.downcast_ref::<CompassState>();
        {
            let mut paras = state.paragraphs.borrow_mut();
            paras.clear();
            for t in texts.iter() {
                paras.push(Plain::<<R as adv_text::Renderer>::Paragraph>::new(adv_text::Text {
                    content: t.text.clone(),
                    bounds: Size::new(120.0, 20.0),
                    size: Pixels(13.0),
                    line_height: LineHeight::default(),
                    font,
                    align_x: alignment::Horizontal::Left.into(),
                    align_y: alignment::Vertical::Top.into(),
                    shaping: adv_text::Shaping::Basic,
                    wrapping: adv_text::Wrapping::None,
                    ellipsize: adv_text::Ellipsize::None,
                }));
            }
        }
        let paras = state.paragraphs.borrow();
        for (gi, t) in texts.iter().enumerate() {
            let para = &paras[gi];
            let w = para.min_bounds().width;
            let x = if t.centered { t.x - w / 2.0 } else { t.x };
            renderer.fill_paragraph(
                para.raw(),
                Point::new(x.round(), t.y.round()),
                t.color,
                b,
            );
        }
    }

    fn update(
        &mut self,
        tree: &mut Tree,
        event: &Event,
        layout: Layout<'_>,
        cursor: Cursor,
        _renderer: &R,
        _clipboard: &mut dyn Clipboard,
        shell: &mut Shell<'_, M>,
        _viewport: &Rectangle,
    ) {
        let state = tree.state.downcast_mut::<CompassState>();
        let b = layout.bounds();
        let proj = projection_from_type(self.ptype);

        match event {
            Event::Mouse(mouse::Event::ButtonPressed(Button::Left)) => {
                let Some(p) = cursor.position_over(b) else { return };
                state.drag = Some((p.x, self.center_deg));
                if self.style == CompassStyle::Ruler {
                    let alph = ((p.x - b.x) / b.width * 360.0) as f64;
                    shell.publish((self.on_change)(alph));
                }
            }
            Event::Mouse(mouse::Event::ButtonReleased(Button::Left)) => {
                state.drag = None;
            }
            Event::Mouse(mouse::Event::CursorMoved { .. }) => {
                let Some((x0, c0)) = state.drag else { return };
                let Some(p) = cursor.position() else { return };
                match self.style {
                    CompassStyle::Aligned => {
                        let cx = self.center_x(b);
                        let z = self.zoom;
                        let a0 =
                            self.alph_at_img_x(&*proj, ((x0 - cx) / z) as f64, c0.to_radians());
                        let a1 =
                            self.alph_at_img_x(&*proj, ((p.x - cx) / z) as f64, c0.to_radians());
                        let new_c = (c0.to_radians() - (a1 - a0)).to_degrees();
                        shell.publish((self.on_change)(new_c));
                    }
                    CompassStyle::Ruler => {
                        let alph = ((p.x - b.x) / b.width * 360.0) as f64;
                        shell.publish((self.on_change)(alph));
                    }
                    CompassStyle::Off => {}
                }
            }
            _ => {}
        }
    }

    fn mouse_interaction(
        &self,
        _tree: &Tree,
        layout: Layout<'_>,
        cursor: Cursor,
        _viewport: &Rectangle,
        _renderer: &R,
    ) -> Interaction {
        if cursor.position_over(layout.bounds()).is_some() {
            Interaction::Grab
        } else {
            Interaction::None
        }
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
        // realistic magnitudes: x0 is in normalized (pre-scale) units
        roundtrip(ProjectionType::Rectilinear, 0.002, -0.0003, 0.01);
    }
    #[test]
    fn inverse_cylindrical() {
        roundtrip(ProjectionType::Cylindrical, 0.0, 0.0, 0.0);
    }
}
