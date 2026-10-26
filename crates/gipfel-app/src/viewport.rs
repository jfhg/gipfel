//! The gipfel viewport: a custom widget that draws the photo, hill markers,
//! decluttered labels, GPS track, and handles all canvas interaction.
//!
//! Marker/label placement mirrors the original GipfelWidget::draw/set_labels
//! (image-space coordinates; the screen transform is applied at draw time).

use std::rc::Rc;
use std::time::Instant;

use cosmic::iced::advanced::image::{FilterMethod, Handle, Image as CoreImage};
use cosmic::iced::advanced::renderer::Style as RendererStyle;
use cosmic::iced::advanced::text::{
    self as adv_text, paragraph::Plain, LineHeight, Renderer as _,
};
use cosmic::iced::advanced::widget::{tree, Tree, Widget as WidgetTrait};
use cosmic::iced::advanced::{Clipboard, Layout, Shell};
use cosmic::iced::mouse::{self, Button, Cursor, Interaction, ScrollDelta};
use cosmic::iced::widget::canvas::{Frame, LineCap, Path, Stroke};
use cosmic::iced::advanced::graphics::geometry::Style as GeoStyle;
use cosmic::iced::{
    alignment, Color, Event, Length, Pixels, Point, Rectangle, Radians, Size, Vector,
};

use cosmic::Renderer as R;
use cosmic::Theme;

pub const DEFAULT_LABEL_FONT: f32 = 12.0;
const CROSS_SIZE: f64 = 4.0;

#[derive(Clone, Debug)]
pub struct Marker {
    /// image-space coordinates, origin at image center (like C's m->x/m->y)
    pub x: f64,
    pub y: f64,
    pub name: Rc<str>,
    pub idx: usize,
    pub known: bool,
    pub hidden: bool,
}

#[derive(Clone, Debug)]
pub struct TrackPt {
    pub x: f64,
    pub y: f64,
    pub hidden: bool,
    pub width: f64,
}

#[derive(Debug, Clone)]
pub enum VpMsg {
    /// zoom by `factor`, keeping image point `img` under the cursor
    ZoomAt { factor: f32, img: Point },
    PanBy { delta: Vector },
    PanTo { pan: Vector },
    MoveMarker { idx: usize, img: Point },
    DragEnd { had_marker: bool },
    /// hill index of the toggled marker (None = clicked empty space)
    ToggleKnown(Option<usize>),
    ContextAt { img: Point, hit: Option<usize> },
    /// hill index of the hovered marker
    Hover(Option<usize>),
}

#[derive(Clone)]
struct LabelLayout {
    version: u64,
    font_size: f32,
    zoom: f32,
    entries: Vec<(f32, f32)>,
}

#[derive(Default)]
struct DragState {
    drag: Drag,
    last_click: Option<(Instant, Point)>,
    label_cache: std::cell::RefCell<Option<LabelLayout>>,
    /// Paragraph primitives must outlive the frame: the renderer stores only a
    /// Weak reference, so keep strong refs here until the next draw.
    paragraphs: std::cell::RefCell<Vec<Plain<<R as adv_text::Renderer>::Paragraph>>>,
}

#[derive(Default, Clone)]
enum Drag {
    #[default]
    None,
    Marker(usize),
    Pan { start: Point, pan0: Vector },
}

pub struct ViewportWidget<M: From<VpMsg> + Send + 'static> {
    pub handle: Handle,
    pub img_w: f32,
    pub img_h: f32,
    pub zoom: f32,
    pub pan: Vector,
    /// hill markers in visible order; label layout depends on this order
    pub markers: Rc<Vec<Marker>>,
    pub track: Rc<Vec<TrackPt>>,
    pub hover: Option<usize>,
    pub ctrl: bool,
    /// label font size in pixels (UI-tunable)
    pub font_size: f32,
    /// bump whenever marker positions change to invalidate the label cache
    pub version: u64,
    pub _msg: std::marker::PhantomData<fn() -> M>,
}

impl<M: From<VpMsg> + Send + 'static> Clone for ViewportWidget<M> {
    fn clone(&self) -> Self {
        ViewportWidget {
            handle: self.handle.clone(),
            img_w: self.img_w,
            img_h: self.img_h,
            zoom: self.zoom,
            pan: self.pan,
            markers: self.markers.clone(),
            track: self.track.clone(),
            hover: self.hover,
            ctrl: self.ctrl,
            font_size: self.font_size,
            version: self.version,
            _msg: std::marker::PhantomData,
        }
    }
}

impl<M: From<VpMsg> + Send + 'static> ViewportWidget<M> {
    /// line height: paragraph metrics (cosmic LineHeight default = 1.3x) plus
    /// a 2px gap, so stacked labels never touch
    fn line_h(&self) -> f32 {
        self.font_size * 1.3 + 2.0
    }

    fn center(bounds: Rectangle, pan: Vector) -> Point {
        Point::new(
            bounds.x + bounds.width / 2.0 + pan.x,
            bounds.y + bounds.height / 2.0 + pan.y,
        )
    }

    fn to_img(&self, center: Point, p: Point) -> Point {
        Point::new((p.x - center.x) / self.zoom, (p.y - center.y) / self.zoom)
    }

    /// hit-test in image space; tolerance scales with zoom (~6 screen px).
    /// Returns the *hill index* of the hit marker.
    fn hit(&self, center: Point, p: Point) -> Option<usize> {
        let img = self.to_img(center, p);
        let tol = (6.0 / self.zoom).clamp(2.0, 20.0);
        self.markers
            .iter()
            .find(|m| (m.x as f32 - img.x).abs() < tol && (m.y as f32 - img.y).abs() < tol)
            .map(|m| m.idx)
    }
}

impl<M: From<VpMsg> + Send + 'static> WidgetTrait<M, Theme, R> for ViewportWidget<M> {
    fn size(&self) -> Size<Length> {
        Size::new(Length::Fill, Length::Fill)
    }

    fn tag(&self) -> tree::Tag {
        tree::Tag::of::<DragState>()
    }

    fn state(&self) -> tree::State {
        tree::State::new(DragState::default())
    }

    fn layout(
        &mut self,
        _tree: &mut Tree,
        _renderer: &R,
        limits: &cosmic::iced::advanced::layout::Limits,
    ) -> cosmic::iced::advanced::layout::Node {
        cosmic::iced::advanced::layout::Node::new(limits.max())
    }

    fn draw(
        &self,
        tree: &Tree,
        renderer: &mut R,
        _theme: &Theme,
        _style: &RendererStyle,
        layout: Layout<'_>,
        _cursor: Cursor,
        _viewport: &Rectangle,
    ) {
        use cosmic::iced::advanced::image::Renderer as _;
        use cosmic::iced::advanced::Renderer as _;

        let bounds = layout.bounds();
        let center = Self::center(bounds, self.pan);
        let z = self.zoom;

        // 1. photo
        let dest = Rectangle::new(
            Point::new(center.x - self.img_w * z / 2.0, center.y - self.img_h * z / 2.0),
            Size::new(self.img_w * z, self.img_h * z),
        );
        renderer.draw_image(
            CoreImage {
                handle: self.handle.clone(),
                filter_method: FilterMethod::Linear,
                rotation: Radians(0.0),
                border_radius: 0.0.into(),
                opacity: 1.0,
                snap: false,
            },
            dest,
            bounds,
        );

        let layout_entries = self.label_layout(tree, renderer);

        // The wgpu renderer draws geometry before images within one layer;
        // record the overlay in a fresh layer so it lands on top of the photo.
        renderer.start_layer(bounds);
        self.draw_overlay(renderer, tree, bounds, center, &layout_entries);
        renderer.end_layer();
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
        let bounds = layout.bounds();
        let center = Self::center(bounds, self.pan);
        let state = tree.state.downcast_mut::<DragState>();

        match event {
            Event::Mouse(mouse::Event::WheelScrolled { delta }) => {
                let Some(p) = cursor.position_over(bounds) else { return };
                let (dx, dy) = match delta {
                    ScrollDelta::Pixels { x, y } => (*x, *y),
                    ScrollDelta::Lines { x, y } => (*x * 30.0, *y * 30.0),
                };
                if self.ctrl {
                    shell.publish(M::from(VpMsg::ZoomAt {
                        factor: (1.0 + dy * 0.0015).clamp(0.5, 2.0),
                        img: self.to_img(center, p),
                    }));
                } else {
                    shell.publish(M::from(VpMsg::PanBy { delta: Vector::new(-dx, -dy) }));
                }
                shell.request_redraw();
            }
            Event::Mouse(mouse::Event::ButtonPressed(Button::Left)) => {
                let Some(p) = cursor.position_over(bounds) else { return };
                let now = Instant::now();
                let dbl = state
                    .last_click
                    .is_some_and(|(t, q)| {
                        t.elapsed().as_millis() < 400
                            && (q.x - p.x).abs() < 6.0
                            && (q.y - p.y).abs() < 6.0
                    });
                state.last_click = Some((now, p));

                if dbl {
                    state.drag = Drag::None;
                    state.last_click = None;
                    shell.publish(M::from(VpMsg::ToggleKnown(self.hit(center, p))));
                    return;
                }

                if let Some(idx) = self.hit(center, p) {
                    if self.markers.iter().any(|m| m.idx == idx && m.known) {
                        state.drag = Drag::Marker(idx);
                        return;
                    }
                }
                state.drag = Drag::Pan { start: p, pan0: self.pan };
            }
            Event::Mouse(mouse::Event::ButtonReleased(Button::Left)) => {
                let had_marker = matches!(state.drag, Drag::Marker(_));
                state.drag = Drag::None;
                shell.publish(M::from(VpMsg::DragEnd { had_marker }));
            }
            Event::Mouse(mouse::Event::ButtonPressed(Button::Middle)) => {
                if let Some(p) = cursor.position_over(bounds) {
                    shell.publish(M::from(VpMsg::ToggleKnown(self.hit(center, p))));
                }
            }
            Event::Mouse(mouse::Event::ButtonPressed(Button::Right)) => {
                if let Some(p) = cursor.position_over(bounds) {
                    shell.publish(M::from(VpMsg::ContextAt {
                        img: self.to_img(center, p),
                        hit: self.hit(center, p),
                    }));
                }
            }
            Event::Mouse(mouse::Event::CursorMoved { .. }) => {
                match state.drag.clone() {
                    Drag::Marker(idx) => {
                        if let Some(p) = cursor.position() {
                            shell.publish(M::from(VpMsg::MoveMarker {
                                idx,
                                img: self.to_img(center, p),
                            }));
                            shell.request_redraw();
                        }
                    }
                    Drag::Pan { start, pan0 } => {
                        if let Some(p) = cursor.position() {
                            shell.publish(M::from(VpMsg::PanTo {
                                pan: pan0 + Vector::new(p.x - start.x, p.y - start.y),
                            }));
                            shell.request_redraw();
                        }
                    }
                    Drag::None => {
                        match cursor.position_over(bounds) {
                            Some(p) => shell.publish(M::from(VpMsg::Hover(self.hit(center, p)))),
                            // cursor moved off the viewport (e.g. into the
                            // sidebar): clear any stale hover info
                            None => shell.publish(M::from(VpMsg::Hover(None))),
                        }
                    }
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
        let center = Self::center(layout.bounds(), self.pan);
        match cursor.position_over(layout.bounds()) {
            Some(p) if self.hit(center, p).is_some() => Interaction::Pointer,
            Some(_) => Interaction::Grab,
            None => Interaction::None,
        }
    }
}

impl<M: From<VpMsg> + Send + 'static> ViewportWidget<M> {
    fn draw_overlay(
        &self,
        renderer: &mut R,
        tree: &Tree,
        bounds: Rectangle,
        center: Point,
        layout_entries: &[(f32, f32)],
    ) {
        use cosmic::iced::advanced::graphics::geometry::Renderer as _;
        use cosmic::iced::advanced::text::Renderer as _;

        let z = self.zoom;
        let to_screen = |p: Point| Point::new(center.x + p.x * z, center.y + p.y * z);
        let layout_entries = layout_entries;
        let mut frame = Frame::with_bounds(renderer, bounds);

        // 2. GPS track
        for w in self.track.windows(2) {
            let (a, b) = (&w[0], &w[1]);
            let pa = to_screen(Point::new(a.x as f32, a.y as f32));
            let pb = to_screen(Point::new(b.x as f32, b.y as f32));
            let color = if a.hidden {
                Color::from_rgb(0.2, 0.4, 1.0)
            } else {
                Color::from_rgb(1.0, 0.2, 0.2)
            };
            let stroke = Stroke {
                width: (a.width as f32 * z).max(1.0),
                style: GeoStyle::Solid(color),
                line_cap: LineCap::Round,
                ..Default::default()
            };
            let path = Path::new(|bld| {
                bld.move_to(pa);
                bld.line_to(pb);
            });
            frame.stroke(&path, stroke);
        }

        // 3. hill markers: crosses + leaders
        // C's draw() skips markers outside the image (fabs(x) > w/2 || ...);
        // must match label_layout's filter, otherwise off-image markers get
        // drawn (in the letterbox area) without ever being laid out.
        let in_img = |m: &Marker| {
            m.x.abs() * 2.0 <= self.img_w as f64 && m.y.abs() * 2.0 <= self.img_h as f64
        };
        let yellow = Color::from_rgb(1.0, 0.9, 0.2);
        for (i, m) in self.markers.iter().enumerate() {
            if !in_img(m) {
                continue;
            }
            let p = to_screen(Point::new(m.x as f32, m.y as f32));
            if !bounds.contains(p) {
                continue;
            }
            let (lx, ly) = layout_entries[i];
            let d = CROSS_SIZE as f32;
            let label_line = p.y + ly - self.line_h();
            let stroke = Stroke {
                width: 1.0,
                style: GeoStyle::Solid(yellow),
                ..Default::default()
            };
            let path = Path::new(|bld| {
                bld.move_to(Point::new(p.x - d, p.y));
                bld.line_to(Point::new(p.x + d, p.y));
                bld.move_to(Point::new(p.x, p.y));
                bld.line_to(Point::new(p.x, label_line));
                bld.move_to(Point::new(p.x, label_line));
                bld.line_to(Point::new(p.x + lx, label_line));
            });
            frame.stroke(&path, stroke);
        }

        // known-hill flags (pole + pennant + circle)
        let flag_color = Color::from_rgb(1.0, 0.25, 0.25);
        for m in self.markers.iter().filter(|m| m.known) {
            if !in_img(m) {
                continue;
            }
            let p = to_screen(Point::new(m.x as f32, m.y as f32));
            if !bounds.contains(p) {
                continue;
            }
            let stroke = Stroke {
                width: 1.5,
                style: GeoStyle::Solid(flag_color),
                ..Default::default()
            };
            let pole = Path::new(|bld| {
                bld.move_to(p);
                bld.line_to(Point::new(p.x, p.y - 16.0));
                bld.circle(p, 3.0);
            });
            let pennant = Path::new(|bld| {
                bld.move_to(Point::new(p.x, p.y - 16.0));
                bld.line_to(Point::new(p.x + 10.0, p.y - 13.0));
                bld.line_to(Point::new(p.x, p.y - 10.0));
                bld.close();
            });
            frame.stroke(&pole, stroke.clone());
            frame.fill(&pennant, flag_color);
        }

        renderer.draw_geometry(frame.into_geometry());

        // 4. labels (screen space, constant font size)
        let font = renderer.default_font();
        let size = Pixels(self.font_size);
        // the renderer keeps only a Weak ref to each paragraph, so hold strong
        // refs in the widget state until the next frame replaces them
        let state = tree.state.downcast_ref::<DragState>();
        {
            let mut paras = state.paragraphs.borrow_mut();
            paras.clear();
            paras.reserve(self.markers.len());
            for m in self.markers.iter() {
                paras.push(self.make_paragraph(font, size, &m.name));
            }
        }
        let paras = state.paragraphs.borrow();

        for (i, m) in self.markers.iter().enumerate() {
            if !in_img(m) {
                continue;
            }
            let p = to_screen(Point::new(m.x as f32, m.y as f32));
            if !bounds.contains(p) {
                continue;
            }
            let (_, ly) = layout_entries[i];
            let pos = Point::new(p.x + 2.0, p.y + ly - self.line_h());
            let is_hover = self.hover == Some(m.idx);

            let para = &paras[i];
            let mb = para.min_bounds();

            let mut bg = Frame::with_bounds(renderer, bounds);
            let rect = Rectangle::new(
                Point::new(p.x + 1.0, pos.y - 1.0),
                Size::new(mb.width + 2.0, mb.height + 1.0),
            );
            let path = Path::new(|bld| bld.rectangle(rect.position(), rect.size()));
            bg.fill(
                &path,
                if is_hover {
                    Color::from_rgba(1.0, 0.9, 0.2, 0.95)
                } else {
                    Color::from_rgba(0.0, 0.0, 0.0, 0.55)
                },
            );
            renderer.draw_geometry(bg.into_geometry());

            let color = if is_hover {
                Color::BLACK
            } else if m.hidden {
                Color::from_rgb(0.4, 0.6, 1.0)
            } else {
                Color::WHITE
            };
            renderer.fill_paragraph(
                para.raw(),
                Point::new(p.x + 2.0, pos.y),
                color,
                bounds,
            );
        }
    }

    fn make_paragraph(&self, font: <R as adv_text::Renderer>::Font, size: Pixels, text: &str) -> Plain<<R as adv_text::Renderer>::Paragraph> {
        Plain::<<R as adv_text::Renderer>::Paragraph>::new(adv_text::Text {
            content: text.to_string(),
            bounds: Size::new(600.0, 40.0),
            size,
            line_height: LineHeight::default(),
            font,
            align_x: alignment::Horizontal::Left.into(),
            align_y: alignment::Vertical::Top.into(),
            shaping: adv_text::Shaping::Basic,
            wrapping: adv_text::Wrapping::None,
            ellipsize: adv_text::Ellipsize::None,
        })
    }

    /// C set_labels port: place labels above crosses, pushing colliding ones up.
    /// Markers outside the image are skipped entirely (C: fabs(x) > w/2 filter),
    /// so off-image peaks never push visible labels up.
    fn label_layout(&self, tree: &Tree, renderer: &mut R) -> Vec<(f32, f32)> {
        let state = tree.state.downcast_ref::<DragState>();
        if let Some(l) = state.label_cache.borrow().as_ref() {
            if l.version == self.version
                && l.font_size == self.font_size
                && l.zoom == self.zoom
                && l.entries.len() == self.markers.len()
            {
                return l.entries.clone();
            }
        }

        let font = renderer.default_font();
        let size = Pixels(self.font_size);
        let line_h = self.line_h();

        let widths: Vec<f32> = self
            .markers
            .iter()
            .map(|m| self.make_paragraph(font, size, &m.name).min_bounds().width)
            .collect();

        // C: fabs(m->x) > img->w()/2 || fabs(m->y) > img->h()/2 -> skip
        // (marker coords are center-relative, like C's m->x/m->y)
        let in_img: Vec<bool> = self
            .markers
            .iter()
            .map(|m| m.x.abs() * 2.0 <= self.img_w as f64 && m.y.abs() * 2.0 <= self.img_h as f64)
            .collect();

        // Labels are drawn at constant size in *screen* space, so collisions
        // depend on the on-screen distance between markers: run the layout on
        // zoom-scaled coordinates (C had no zoom, image space == screen space).
        let z = self.zoom;
        let sx: Vec<f32> = self.markers.iter().map(|m| m.x as f32 * z).collect();
        let sy: Vec<f32> = self.markers.iter().map(|m| m.y as f32 * z).collect();

        let entries = layout_label_y(&sx, &sy, &widths, &in_img, line_h);
        *state.label_cache.borrow_mut() = Some(LabelLayout {
            version: self.version,
            font_size: self.font_size,
            zoom: self.zoom,
            entries: entries.clone(),
        });
        entries
    }
}

/// Shared label collision-avoidance layout, used for both the on-screen
/// viewport and the screen dump so the dump shows exactly the same label
/// placement (plan.md M3: same code path).
///
/// `xs`/`ys` are marker positions (center-relative) in one uniform pixel
/// space, `widths` the label widths and `line_h` the line height, all in
/// that same space. Markers must be ordered by x (the sweep breaks on the
/// first non-overlapping predecessor). Returns `(label_width + 1, y_offset)`
/// per marker; the label is drawn at `(marker.x + 2, marker.y + y_offset -
/// line_h)`.
pub fn layout_label_y(
    xs: &[f32],
    ys: &[f32],
    widths: &[f32],
    in_img: &[bool],
    line_h: f32,
) -> Vec<(f32, f32)> {
    let n = xs.len();
    let mut label_x = vec![0.0f32; n];
    let mut label_y = vec![0.0f32; n];
    let cross = CROSS_SIZE as f32;
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
        // C does a single overwrite pass here, which can leave overlaps
        // when a push moves the label into a band that was already
        // checked. Iterate to a fixed point instead (converges quickly:
        // each round only moves the label up, bounded by the stack).
        for _round in 0..64 {
            let mut pushed = false;
            for &j in &colliding {
                if overlap(
                    ys[i] + label_y[i] - line_h,
                    line_h,
                    ys[j] + label_y[j] - line_h,
                    line_h,
                ) || overlap(ys[i] + label_y[i] - line_h, line_h, ys[j] - cross, 2.0 * cross) {
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
