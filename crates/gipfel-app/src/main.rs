//! gipfel for the COSMIC desktop: annotate mountain panoramas.
//!
//! M2/M3: full app skeleton (menus, dialogs, settings sidebar) plus the
//! interactive viewport (marker placement, refit, context menu, track).

mod compass;
mod dump;
mod viewport;

use std::marker::PhantomData;
use std::path::{Path, PathBuf};
use std::rc::Rc;

use cosmic::app::{Core, Task};
use cosmic::iced::keyboard::{self, Key};
use cosmic::iced::window::Event as WinEvent;
use cosmic::iced::{event, Color, Event, Length, Subscription, Vector};
use cosmic::widget::{self, menu, menu::action::MenuAction};
use cosmic::{Application, ApplicationExt, Element, Theme};

use gipfel_core::hill::{self, Hill, FLAG_DUPLIC, FLAG_HIDDEN, FLAG_TRACK_POINT, FLAG_VISIBLE};
use gipfel_core::metadata::ImageMetaData;
use gipfel_core::panorama::Panorama;
use gipfel_core::projection::ProjectionType;
use gipfel_core::stitch;
use gipfel_core::track;

use compass::{Compass, CompassStyle, STRIP_H};

use viewport::{Marker, TrackPt, ViewportWidget, VpMsg};

/// actions of the right-click popover menu on the viewport.
/// The click position / hit index are baked into the action at menu-build
/// time (like cosmic-files bakes its entity), so the handler never depends
/// on state that may have changed while the menu was open.
#[derive(Clone, Copy, PartialEq)]
pub enum CtxAction {
    /// image-space position of the right-click
    FindPeak(f64, f64),
    /// hill index of the clicked marker
    ToggleHidden(usize),
}

impl Eq for CtxAction {}

impl MenuAction for CtxAction {
    type Message = Message;

    fn message(&self) -> Message {
        match self {
            CtxAction::FindPeak(x, y) => Message::ContextFindPeak(*x, *y),
            CtxAction::ToggleHidden(i) => Message::ContextToggleHidden(*i),
        }
    }
}

pub struct Flags {
    pub data: Option<PathBuf>,
    pub image: Option<PathBuf>,
}

pub struct Gipfel {
    core: Core,
    pan: Panorama,
    project: Option<Project>,
    /// hill indices of the known (flagged) markers
    known: Vec<usize>,
    /// hill indices of the GPS track points, in file order
    track: Vec<usize>,
    // viewport state
    zoom: f32,
    fit: bool,
    panv: Vector,
    hover: Option<usize>,
    ctrl: bool,
    version: u64,
    win_w: f64,
    win_h: f64,
    vp_w: f64,
    vp_h: f64,
    // sidebar state
    lat_in: String,
    lon_in: String,
    height_in: String,
    k0_in: String,
    k1_in: String,
    x0_in: String,
    profile_in: String,
    profiles: Vec<String>,
    profile_overwrite: bool,
    track_width: f64,
    show_hidden: bool,
    fit_distortion: bool,
    label_font: f32,
    compass_style: CompassStyle,
    status: String,
    // overlays
    picker: Option<Picker>,
    context: Option<ContextState>,
    /// peak info shown in the status bar while the mouse hovers a marker
    hover_info: Option<String>,
    about: bool,
}

struct Project {
    path: PathBuf,
    handle: cosmic::widget::image::Handle,
    w: u32,
    h: u32,
    md: ImageMetaData,
}

struct Picker {
    title: String,
    query: String,
    mode: PickerMode,
}

#[derive(Clone, Copy)]
enum PickerMode {
    Viewpoint,
    FindPeak { img_x: f64, img_y: f64 },
}

struct ContextState {
    img_x: f64,
    img_y: f64,
    hit: Option<usize>,
}

#[derive(Debug, Clone)]
pub enum Message {
    Vp(VpMsg),
    Event(Event),
    WinResized(f64, f64),
    FileDropped(Vec<PathBuf>),

    OpenImage,
    OpenPath(Option<PathBuf>),
    ImageLoaded(Box<Result<LoadedImage, String>>),
    SaveImage,
    SavePath(Option<PathBuf>),
    ImageSaved(Option<PathBuf>),
    DumpImage,
    ImageDumped(Box<Result<PathBuf, String>>),
    StitchTiff,
    StitchDone(Box<Result<PathBuf, String>>),
    LoadTrack,
    TrackLoaded(Box<Result<Vec<Hill>, String>>),

    ChooseViewpoint,
    PickerQuery(String),
    PickerPick(usize),
    PickerClose,
    ContextFindPeak(f64, f64),
    ContextToggleHidden(usize),
    Surface(cosmic::surface::Action<Message>),
    Redraw,

    Center(f64),
    Pitch(f64),
    Roll(f64),
    LabelFont(f32),
    Focal(f64),
    Visibility(f64),
    TrackWidth(f64),
    Lat(String),
    Lon(String),
    Height(String),
    ViewApply,
    K0(String),
    K1(String),
    X0(String),
    DistortionApply,
    ProfileName(String),
    ProfileSave,
    ProfileLoad,
    ProfileSelect(usize),
    Projection(ProjectionType),
    ToggleShowHidden,
    FitDistortion(bool),
    ToggleNav,
    CompassStyle(CompassStyle),

    Fit,
    ZoomIn,
    ZoomOut,
    About(bool),
    Quit,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum Action {
    Open,
    Save,
    Dump,
    Stitch,
    LoadTrack,
    Quit,
    ChooseViewpoint,
    ShowHidden,
    Fit,
    ZoomIn,
    ZoomOut,
    ProjNormal,
    ProjPanoramic,
    CompassAligned,
    CompassRuler,
    CompassOff,
    About,
}

impl MenuAction for Action {
    type Message = Message;
    fn message(&self) -> Message {
        match self {
            Action::Open => Message::OpenImage,
            Action::Save => Message::SaveImage,
            Action::Dump => Message::DumpImage,
            Action::Stitch => Message::StitchTiff,
            Action::LoadTrack => Message::LoadTrack,
            Action::Quit => Message::Quit,
            Action::ChooseViewpoint => Message::ChooseViewpoint,
            Action::ShowHidden => Message::ToggleShowHidden,
            Action::Fit => Message::Fit,
            Action::ZoomIn => Message::ZoomIn,
            Action::ZoomOut => Message::ZoomOut,
            Action::ProjNormal => Message::Projection(ProjectionType::Rectilinear),
            Action::ProjPanoramic => Message::Projection(ProjectionType::Cylindrical),
            Action::CompassAligned => Message::CompassStyle(CompassStyle::Aligned),
            Action::CompassRuler => Message::CompassStyle(CompassStyle::Ruler),
            Action::CompassOff => Message::CompassStyle(CompassStyle::Off),
            Action::About => Message::About(true),
        }
    }
}

impl From<VpMsg> for Message {
    fn from(m: VpMsg) -> Self {
        Message::Vp(m)
    }
}

#[derive(Clone)]
pub struct LoadedImage {
    path: PathBuf,
    w: u32,
    h: u32,
    handle: cosmic::widget::image::Handle,
    md: ImageMetaData,
}

impl std::fmt::Debug for LoadedImage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "LoadedImage({})", self.path.display())
    }
}

/// Load a GPS track from a `.gpx` (GPS Exchange) or `.csv`/`.dat` (gipfel
/// hill-CSV) file, returning hills flagged as track points.
fn load_track_file(path: &Path) -> Result<Vec<Hill>, String> {
    if path.extension().is_some_and(|e| e.eq_ignore_ascii_case("gpx")) {
        track::load_gpx(&path.to_string_lossy(), 2000)
    } else {
        let mut hs = hill::load(&path.to_string_lossy()).map_err(|e| e.to_string())?;
        for h in hs.iter_mut() {
            h.flags |= FLAG_TRACK_POINT;
        }
        Ok(hs)
    }
}

fn data_candidates(flag: Option<&Path>) -> Vec<PathBuf> {
    let mut v = Vec::new();
    if let Some(p) = flag {
        v.push(p.to_path_buf());
    }
    if let Ok(e) = std::env::var("GIPFEL_DATA") {
        v.push(PathBuf::from(e));
    }
    v.push(PathBuf::from("gipfel.dat"));
    if let Some(home) = std::env::var_os("HOME") {
        v.push(PathBuf::from(home).join(".local/share/gipfel/gipfel.dat"));
    }
    v.push(PathBuf::from("/usr/local/share/gipfel/gipfel.dat"));
    v.push(PathBuf::from("/usr/share/gipfel/gipfel.dat"));
    v
}

/// Inverse projection for the vertical axis: the elevation whose projected
/// pixel row (image-centered coords, center column) is `y_img`. Shares the
/// damped Newton iteration with the compass strip (`compass::newton_inverse`).
fn elev_at_img_y(pan: &Panorama, y_img: f64) -> f64 {
    let c = pan.parms.a_center;
    let p = pan.parms.a_pitch;
    let seed = p - (y_img / pan.parms.scale).atan();
    compass::newton_inverse(
        y_img,
        |e| pan.project_point(c, e).map(|v| v.1),
        seed,
        p - 1.535,
        p + 1.535,
    )
}

impl Gipfel {
    fn bump(&mut self) {
        // keep the visibility limit in sync with the lens' actual field of
        // view: the angle at which the (distorted) projection reaches the
        // image edge. Directions beyond it cannot be inside the photo, even
        // if the distortion model folds their projection back into it.
        let limit = self.view_angle_limit();
        if (limit - self.pan.max_view_angle).abs() > 1e-9 {
            self.pan.max_view_angle = limit;
            // keep the user-placed marker positions (observations) intact
            self.pan.update_visible_mountains(Some(&self.known));
        }
        self.version += 1;
    }

    /// Reload the stored profile names from the prefs file.
    fn refresh_profiles(&mut self) {
        self.profiles = gipfel_core::profile::list().into_iter().map(|(n, _)| n).collect();
    }

    /// Load a stored profile and apply its distortion parameters.
    fn apply_profile(&mut self, name: &str) {
        match gipfel_core::profile::load(name) {
            Ok(p) => {
                self.pan.set_distortion_params(p.k0, p.k1, p.x0);
                self.sync_inputs();
                self.bump();
                self.status = format!("loaded profile '{name}'");
            }
            Err(e) => self.status = format!("profile '{name}': {e}"),
        }
    }

    /// half-angle (radians) the projection maps the image's right edge to,
    /// i.e. the horizontal FOV of the lens including fitted distortion.
    /// 0 = no image loaded / not computable.
    fn view_angle_limit(&self) -> f64 {
        let Some(p) = &self.project else {
            return 0.0;
        };
        let proj = gipfel_core::projection::projection_from_type(self.pan.projection_type);
        let c = self.pan.parms.a_center;
        let a_edge = compass::alph_at_img_x(
            self.pan.projection_type,
            &*proj,
            &self.pan.parms,
            p.w as f64 / 2.0,
            c,
        );
        (a_edge - c).abs()
    }

    fn img_max(&self) -> f64 {
        self.project
            .as_ref()
            .map(|p| p.w.max(p.h) as f64)
            .unwrap_or(1000.0)
    }

    fn focal_35mm(&self) -> f64 {
        self.pan.get_scale() * 35.0 / self.img_max()
    }

    fn set_focal_35mm(&mut self, f: f64) {
        self.pan.set_scale(f * self.img_max() / 35.0);
    }

    fn sync_inputs(&mut self) {
        self.lat_in = format!("{:.5}", self.pan.get_view_lat());
        self.lon_in = format!("{:.5}", self.pan.get_view_long());
        self.height_in = format!("{:.0}", self.pan.get_view_height());
        self.k0_in = format!("{:.5}", self.pan.parms.k0);
        self.k1_in = format!("{:.5}", self.pan.parms.k1);
        self.x0_in = format!("{:.1}", self.pan.parms.x0);
    }

    fn fit_zoom(&self) -> f32 {
        let p = match &self.project {
            Some(p) => p,
            None => return 1.0,
        };
        if self.vp_w < 10.0 || self.vp_h < 10.0 {
            return 1.0;
        }
        (self.vp_w / p.w as f64).min(self.vp_h / p.h as f64) as f32
    }

    fn markers(&self) -> Rc<Vec<Marker>> {
        let mut v = Vec::new();
        for &i in &self.pan.visible {
            let h = &self.pan.hills[i];
            if h.is_track_point() || h.flags & FLAG_DUPLIC != 0 {
                continue;
            }
            let hidden = h.flags & FLAG_HIDDEN != 0;
            if hidden && !self.show_hidden {
                continue;
            }
            v.push(Marker {
                x: h.x,
                y: h.y,
                name: h.name.as_str().into(),
                idx: i,
                known: self.known.contains(&i),
                hidden,
            });
        }
        Rc::new(v)
    }

    fn track_pts(&self) -> Rc<Vec<TrackPt>> {
        let v = self
            .track
            .iter()
            .filter(|&&i| self.pan.hills[i].flags & FLAG_VISIBLE != 0)
            .map(|&i| {
                let h = &self.pan.hills[i];
                // C: max(scale * track_width / (dist_m * 10), 1) - distance in
                // meters. (Was wrongly divided by km here, inflating widths
                // 1000x and breaking stroke triangulation.) Clamped high so
                // near-camera points can't produce absurd strokes.
                let dist_m = self.pan.get_real_distance(i).max(1.0);
                let width =
                    (self.pan.get_scale() * self.track_width / (dist_m * 10.0)).clamp(1.0, 1000.0);
                TrackPt {
                    x: h.x,
                    y: h.y,
                    hidden: h.flags & FLAG_HIDDEN != 0,
                    width,
                }
            })
            .collect();
        Rc::new(v)
    }

    /// recompute viewport metrics from window size, nav and compass state
    fn remeasure(&mut self) {
        let nav = if self.core().nav_bar_active() { 340.0 } else { 0.0 };
        let strip = if self.project.is_some() && self.compass_style != CompassStyle::Off {
            STRIP_H as f64
        } else {
            0.0
        };
        self.vp_w = (self.win_w - nav).max(100.0);
        self.vp_h = (self.win_h - 100.0 - strip).max(100.0);
        if self.fit {
            self.zoom = self.fit_zoom();
        }
    }

    fn refit(&mut self) {
        if self.known.is_empty() {
            return;
        }
        if let Err(e) = self.pan.comp_params(&self.known, self.fit_distortion) {
            self.status = format!("refit failed: {e}");
        }
        self.sync_inputs();
        self.bump();
    }

    fn md_from_state(&self) -> ImageMetaData {
        let mut md = ImageMetaData::new();
        if let Some(p) = &self.project {
            md.manufacturer = p.md.manufacturer.clone();
            md.model = p.md.model.clone();
        }
        md.longitude = self.pan.get_view_long();
        md.latitude = self.pan.get_view_lat();
        md.height = self.pan.get_view_height();
        md.direction = self.pan.get_center_angle();
        md.pitch = self.pan.get_pitch_angle();
        md.roll = self.pan.get_roll_angle();
        md.focal_length_35mm = self.focal_35mm();
        md.projection_type = self.pan.projection_type as i32;
        md.k0 = self.pan.parms.k0;
        md.k1 = self.pan.parms.k1;
        md.x0 = self.pan.parms.x0;
        md
    }

    fn open_image(&mut self, path: PathBuf) -> Task<Message> {
        let task = async move {
            let img = image::open(&path).map_err(|e| format!("{}: {e}", path.display()))?;
            let (w, h) = (img.width(), img.height());
            let rgba = img.to_rgba8();
            let handle = cosmic::widget::image::Handle::from_rgba(w, h, rgba.into_raw());
            let mut md = ImageMetaData::new();
            md.load_image(&path.to_string_lossy());
            Ok(LoadedImage { path, w, h, handle, md })
        };
        cosmic::task::future(async move { Message::ImageLoaded(Box::new(task.await)) })
    }

    fn picker_entries(&self, p: &Picker) -> Vec<(String, usize)> {
        let mut idxs: Vec<usize> = match p.mode {
            PickerMode::Viewpoint => (0..self.pan.hills.len()).collect(),
            PickerMode::FindPeak { .. } => self.pan.close.clone(),
        };
        let q = p.query.to_uppercase();
        let mut entries: Vec<(String, usize)> = idxs
            .drain(..)
            .filter(|&i| {
                let h = &self.pan.hills[i];
                h.flags & (FLAG_DUPLIC | FLAG_TRACK_POINT) == 0
                    && (q.is_empty() || h.name.to_uppercase().contains(&q))
            })
            .map(|i| (format!("{} ({} m)", self.pan.hills[i].name, self.pan.hills[i].height as i64), i))
            .collect();
        entries.sort_by(|a, b| a.0.cmp(&b.0));
        entries.truncate(300);
        entries
    }

    fn update_vp(&mut self, msg: VpMsg) -> Task<Message> {
        match msg {
            VpMsg::ZoomAt { factor, img } => {
                let new_zoom = (self.zoom * factor).clamp(0.02, 16.0);
                let z = self.zoom as f64;
                let nz = new_zoom as f64;
                self.panv = Vector::new(
                    self.panv.x + (img.x as f64 * (z - nz)) as f32,
                    self.panv.y + (img.y as f64 * (z - nz)) as f32,
                );
                self.zoom = new_zoom;
                self.fit = false;
            }
            VpMsg::PanBy { delta } => {
                self.panv = Vector::new(self.panv.x + delta.x, self.panv.y + delta.y);
                self.fit = false;
            }
            VpMsg::PanTo { pan } => {
                self.panv = pan;
                self.fit = false;
            }
            VpMsg::MoveMarker { idx, img } => {
                if idx < self.pan.hills.len() {
                    self.pan.hills[idx].x = img.x as f64;
                    self.pan.hills[idx].y = img.y as f64;
                    self.bump();
                }
            }
            VpMsg::DragEnd { had_marker } => {
                if had_marker {
                    self.refit();
                }
            }
            VpMsg::ToggleKnown(hit) => {
                if let Some(i) = hit {
                    if let Some(pos) = self.known.iter().position(|&k| k == i) {
                        self.known.remove(pos);
                    } else {
                        self.known.push(i);
                    }
                    self.refit();
                }
            }
            VpMsg::ContextAt { img, hit } => {
                self.context = Some(ContextState { img_x: img.x as f64, img_y: img.y as f64, hit });
            }
            VpMsg::Hover(h) => {
                if h != self.hover {
                    self.hover = h;
                    self.hover_info = h.map(|i| {
                        let hm = &self.pan.hills[i];
                        format!(
                            "{} ({} m), distance {:.2} km",
                            hm.name,
                            hm.height as i64,
                            self.pan.get_real_distance(i) / 1000.0
                        )
                    });
                }
            }
        }
        Task::none()
    }
}

impl Application for Gipfel {
    type Executor = cosmic::executor::Default;
    type Flags = Flags;
    type Message = Message;
    const APP_ID: &'static str = "org.gipfel.App";

    fn core(&self) -> &Core {
        &self.core
    }

    fn core_mut(&mut self) -> &mut Core {
        &mut self.core
    }

    fn init(core: Core, flags: Flags) -> (Self, Task<Message>) {
        let mut pan = Panorama::new();
        let mut status = String::from("no hill database loaded");
        for c in data_candidates(flags.data.as_deref()) {
            if c.is_file() {
                match pan.load_data(&c.to_string_lossy()) {
                    Ok(()) => {
                        status = format!("{} hills loaded", pan.hills.len());
                        break;
                    }
                    Err(e) => status = format!("cannot read {}: {e}", c.display()),
                }
            }
        }

        let mut app = Gipfel {
            core,
            pan,
            project: None,
            known: Vec::new(),
            track: Vec::new(),
            zoom: 1.0,
            fit: true,
            panv: Vector::ZERO,
            hover: None,
            ctrl: false,
            version: 0,
            win_w: 1280.0,
            win_h: 800.0,
            vp_w: 0.0,
            vp_h: 0.0,
            lat_in: String::new(),
            lon_in: String::new(),
            height_in: String::new(),
            k0_in: String::new(),
            k1_in: String::new(),
            x0_in: String::new(),
            profile_in: String::new(),
            profiles: gipfel_core::profile::list().into_iter().map(|(n, _)| n).collect(),
            profile_overwrite: false,
            track_width: 200.0,
            show_hidden: false,
            label_font: viewport::DEFAULT_LABEL_FONT,
            fit_distortion: false,
            compass_style: CompassStyle::Aligned,
            status,
            picker: None,
            context: None,
            hover_info: None,
            about: false,
        };
        app.sync_inputs();

        let task = match flags.image {
            Some(p) => app.open_image(p),
            None => Task::none(),
        };
        (app, task)
    }

    fn header_start(&self) -> Vec<Element<'_, Message>> {
        let mut kb = std::collections::HashMap::new();
        use cosmic::widget::menu::key_bind::{KeyBind, Modifier};
        let kb = &mut kb;
        for (ch, action) in [
            ("o", Action::Open),
            ("s", Action::Save),
            ("d", Action::Dump),
            ("t", Action::LoadTrack),
            ("v", Action::ChooseViewpoint),
            ("f", Action::Fit),
            ("q", Action::Quit),
        ] {
            kb.insert(
                KeyBind { modifiers: vec![Modifier::Ctrl], key: Key::Character(ch.into()) },
                action,
            );
        }

        let file = menu::Tree::with_children(
            cosmic::widget::RcElementWrapper::new(menu::root("File").into()),
            menu::items(
                kb,
                vec![
                    menu::Item::Button("Open Image…", None, Action::Open),
                    menu::Item::Button("Save Image…", None, Action::Save),
                    menu::Item::Button("Screen Dump…", None, Action::Dump),
                    menu::Item::Button("Stitch Cylindrical TIFF…", None, Action::Stitch),
                    menu::Item::Divider,
                    menu::Item::Button("Load GPX Track…", None, Action::LoadTrack),
                    menu::Item::Divider,
                    menu::Item::Button("Quit", None, Action::Quit),
                ],
            ),
        );
        let view = menu::Tree::with_children(
            cosmic::widget::RcElementWrapper::new(menu::root("View").into()),
            menu::items(
                kb,
                vec![
                    menu::Item::Button("Choose Viewpoint…", None, Action::ChooseViewpoint),
                    menu::Item::CheckBox(
                        "Show Hidden",
                        None,
                        self.show_hidden,
                        Action::ShowHidden,
                    ),
                    menu::Item::Divider,
                    menu::Item::Button("Zoom In", None, Action::ZoomIn),
                    menu::Item::Button("Zoom Out", None, Action::ZoomOut),
                    menu::Item::Button("Fit", None, Action::Fit),
                    menu::Item::Divider,
                    menu::Item::CheckBox(
                        "Compass strip",
                        None,
                        self.compass_style == CompassStyle::Aligned,
                        Action::CompassAligned,
                    ),
                    menu::Item::CheckBox(
                        "Compass ruler",
                        None,
                        self.compass_style == CompassStyle::Ruler,
                        Action::CompassRuler,
                    ),
                    menu::Item::CheckBox(
                        "Compass off",
                        None,
                        self.compass_style == CompassStyle::Off,
                        Action::CompassOff,
                    ),
                ],
            ),
        );
        let projection = menu::Tree::with_children(
            cosmic::widget::RcElementWrapper::new(menu::root("Projection").into()),
            menu::items(
                kb,
                vec![
                    menu::Item::CheckBox(
                        "Normal",
                        None,
                        self.pan.projection_type == ProjectionType::Rectilinear,
                        Action::ProjNormal,
                    ),
                    menu::Item::CheckBox(
                        "Panoramic",
                        None,
                        self.pan.projection_type == ProjectionType::Cylindrical,
                        Action::ProjPanoramic,
                    ),
                ],
            ),
        );
        let help = menu::Tree::with_children(
            cosmic::widget::RcElementWrapper::new(menu::root("Help").into()),
            menu::items(kb, vec![menu::Item::Button("About gipfel", None, Action::About)]),
        );

        vec![
            widget::nav_bar_toggle()
                .active(self.core().nav_bar_active())
                .on_toggle(Message::ToggleNav)
                .into(),
            menu::bar(vec![file, view, projection, help])
                .item_height(menu::ItemHeight::Dynamic(40))
                .item_width(menu::ItemWidth::Uniform(260))
                .spacing(4.0)
                .into(),
        ]
    }

    fn subscription(&self) -> Subscription<Message> {
        event::listen_with(|event, _status, _id| match event {
            Event::Keyboard(keyboard::Event::ModifiersChanged(m)) => {
                Some(Message::Event(Event::Keyboard(keyboard::Event::ModifiersChanged(m))))
            }
            Event::Keyboard(keyboard::Event::KeyPressed { key, modifiers, .. }) => {
                if modifiers.control() {
                    if let Key::Character(c) = &key {
                        match c.as_str() {
                            "o" | "O" => return Some(Message::OpenImage),
                            "s" | "S" => return Some(Message::SaveImage),
                            "d" | "D" => return Some(Message::DumpImage),
                            "t" | "T" => return Some(Message::LoadTrack),
                            "v" | "V" => return Some(Message::ChooseViewpoint),
                            "f" | "F" => return Some(Message::Fit),
                            "q" | "Q" => return Some(Message::Quit),
                            _ => {}
                        }
                    }
                }
                match key {
                    Key::Named(cosmic::iced::keyboard::key::Named::Escape) => {
                        Some(Message::PickerClose)
                    }
                    _ => None,
                }
            }
            Event::Window(WinEvent::FileDropped(paths)) => Some(Message::FileDropped(paths)),
            Event::Window(WinEvent::Resized(size)) => {
                Some(Message::WinResized(size.width as f64, size.height as f64))
            }
            _ => None,
        })
    }

    fn update(&mut self, message: Message) -> Task<Message> {
        use cosmic::dialog::file_chooser::FileFilter;

        match message {
            Message::Vp(m) => return self.update_vp(m),
            Message::Event(Event::Keyboard(keyboard::Event::ModifiersChanged(m))) => {
                self.ctrl = m.control();
            }
            Message::Event(_) => {}
            Message::ToggleNav => {
                self.core_mut().nav_bar_toggle();
                self.remeasure();
            }
            Message::WinResized(w, h) => {
                self.win_w = w;
                self.win_h = h;
                self.remeasure();
            }
            Message::FileDropped(paths) => {
                for p in paths {
                    let ext = p.extension().and_then(|e| e.to_str()).unwrap_or("");
                    if matches!(ext.to_ascii_lowercase().as_str(), "jpg" | "jpeg" | "png") {
                        return self.open_image(p);
                    } else if ext.eq_ignore_ascii_case("csv") || ext.eq_ignore_ascii_case("dat") {
                        // treat as track file
                        let task = async move {
                            hill::load(&p.to_string_lossy())
                                .map(|mut hs| {
                                    for h in hs.iter_mut() {
                                        h.flags |= FLAG_TRACK_POINT;
                                    }
                                    hs
                                })
                                .map_err(|e| e.to_string())
                        };
                        return cosmic::task::future(async move {
                            Message::TrackLoaded(Box::new(task.await))
                        });
                    }
                }
            }
            Message::OpenImage => {
                return cosmic::task::future(async move {
                    let filter =
                        FileFilter::new("Images").glob("*.jpg").glob("*.jpeg").glob("*.png");
                    Message::OpenPath(pick_open("Open Image", filter).await)
                });
            }
            Message::OpenPath(Some(p)) => return self.open_image(p),
            Message::OpenPath(None) => {}
            Message::ImageLoaded(res) => match *res {
                Ok(li) => {
                    let title = li.path.file_name().map(|s| s.to_string_lossy().into_owned()).unwrap_or_default();
                    self.set_header_title(title);
                    let (w, h) = (li.w, li.h);
                    self.project = Some(Project {
                        path: li.path,
                        handle: li.handle,
                        w,
                        h,
                        md: li.md.clone(),
                    });
                    let md = li.md;
                    let have = gipfel_core::metadata::apply_image_metadata(&mut self.pan, &md, w, h);
                    self.known.clear();
                    self.track.clear();
                    self.sync_inputs();
                    self.fit = true;
                    self.remeasure();
                    self.zoom = self.fit_zoom();
                    self.panv = Vector::ZERO;
                    self.bump();
                    // prefill the profile name the auto-match used (C
                    // get_distortion_profile_name) and refresh the store list
                    let auto_profile = gipfel_core::metadata::distortion_profile_name(&md);
                    self.profile_in = auto_profile.clone().unwrap_or_default();
                    self.profile_overwrite = false;
                    self.refresh_profiles();
                    self.status = if have {
                        format!("loaded (gipfel info found)")
                    } else {
                        "loaded (no gipfel info: set viewpoint)".into()
                    };
                    if md.k0.is_nan() {
                        if let Some(name) =
                            auto_profile.filter(|n| self.profiles.iter().any(|p| p == n))
                        {
                            self.status = format!("{} - distortion profile '{name}'", self.status);
                        }
                    }
                    // the first frame after the image appears can be drawn
                    // before the texture is fully uploaded; a late nudge
                    // guarantees the photo shows without user interaction
                    return Task::perform(
                        async { tokio::time::sleep(std::time::Duration::from_millis(150)).await },
                        |_| cosmic::Action::App(Message::Redraw),
                    );
                }
                Err(e) => self.status = e,
            },
            Message::SaveImage => {
                let Some(p) = &self.project else {
                    self.status = "no image loaded".into();
                    return Task::none();
                };
                let _ = p;
                return cosmic::task::future(async move {
                    let filter = FileFilter::new("JPEG").glob("*.jpg");
                    Message::SavePath(pick_save("Save Image", filter).await)
                });
            }
            Message::SavePath(out) => {
                let (Some(out), Some(p)) = (out, &self.project) else { return Task::none() };
                let md = self.md_from_state();
                let src = p.path.clone();
                return cosmic::task::future(async move {
                    match md.save_image(&src.to_string_lossy(), &out.to_string_lossy()) {
                        Ok(()) => Message::ImageSaved(Some(out)),
                        Err(e) => {
                            let _ = e;
                            Message::ImageSaved(None)
                        }
                    }
                });
            }
            Message::ImageSaved(Some(p)) => {
                self.status = format!("saved {}", p.display());
            }
            Message::ImageSaved(None) => {}
            Message::DumpImage => {
                let Some(p) = &self.project else {
                    self.status = "no image loaded".into();
                    return Task::none();
                };
                let path = p.path.clone();
                // the dump is at image resolution; screen-pixel overlay sizes
                // must be divided by the current zoom to look like the screen
                let zoom = self.zoom.max(0.02) as f64;
                let markers: Vec<dump::DumpMarker> = self
                    .markers()
                    .iter()
                    .map(|m| dump::DumpMarker {
                        x: m.x,
                        y: m.y,
                        name: m.name.to_string(),
                        known: m.known,
                        hidden: m.hidden,
                    })
                    .collect();
                let track: Vec<dump::DumpTrackPt> = self
                    .track_pts()
                    .iter()
                    .map(|t| dump::DumpTrackPt { x: t.x, y: t.y, hidden: t.hidden, width: t.width })
                    .collect();
                let font_size = self.label_font;
                let font_family = cosmic::config::interface_font().family;
                return cosmic::task::future(async move {
                    let res: Result<PathBuf, String> = async {
                        let filter = FileFilter::new("Images").glob("*.png").glob("*.jpg");
                        let out =
                            pick_save("Screen Dump", filter).await.ok_or_else(|| "cancelled".to_string())?;
                        let img = image::open(&path)
                            .map(|i| i.to_rgba8())
                            .map_err(|e| format!("{e}"))?;
                        dump::render(&img, &markers, &track, &out, font_size, 1.0 / zoom, &font_family)?;
                        Ok(out)
                    }
                    .await;
                    Message::ImageDumped(Box::new(res))
                });
            }
            Message::ImageDumped(res) => {
                self.status = match *res {
                    Ok(p) => format!("dumped {}", p.display()),
                    Err(e) => e,
                };
            }
            Message::StitchTiff => {
                let Some(p) = &self.project else {
                    self.status = "no image loaded".into();
                    return Task::none();
                };
                // cylindrical output spanning the lens' actual field of view
                let edge = self.view_angle_limit(); // half-angle, radians
                if !(edge > 0.0 && edge < 1.5) {
                    self.status = "cannot determine lens field of view".into();
                    return Task::none();
                }
                let center = self.pan.get_center_angle();
                let (from, to) = (center - edge.to_degrees(), center + edge.to_degrees());
                // output scale = the source's focal scale (pixels per radian
                // at image center): the cylindrical image is 1:1 in the middle
                // and slightly downsampled towards the edges (where the
                // rectilinear source has increasing angular stretch). Using
                // the source width instead would upsample ~5-10% and blur.
                let radius = self.pan.get_scale();
                let w_out = (2.0 * edge * radius).round_ties_even().max(16.0) as u32;
                // cover the source's vertical extent (may be off-center
                // with pitch); output rows are symmetric about elevation 0
                let e_top = elev_at_img_y(&self.pan, -(p.h as f64 / 2.0));
                let e_bot = elev_at_img_y(&self.pan, p.h as f64 / 2.0);
                let e_max = e_top.abs().max(e_bot.abs()).min(85f64.to_radians());
                let h_out = (2.0 * radius * e_max.tan()).round_ties_even().max(16.0) as u32;
                let path = p.path.clone();
                let pan = self.pan.params_snapshot();
                return cosmic::task::future(async move {
                    let res: Result<PathBuf, String> = async {
                        let filter = FileFilter::new("TIFF images").glob("*.tif").glob("*.tiff");
                        let out = pick_save("Stitch Cylindrical TIFF", filter)
                            .await
                            .ok_or_else(|| "cancelled".to_string())?;
                        let out_for_task = out.clone();
                        tokio::task::spawn_blocking(move || -> Result<(), String> {
                            let mut src = stitch::StitchSource::load(&path.to_string_lossy())?;
                            // use the live (possibly adjusted) parameters
                            src.pan = pan;
                            let img = stitch::resample(
                                &[src],
                                stitch::ScanMode::Bicubic,
                                w_out,
                                h_out,
                                from,
                                to,
                            );
                            stitch::write_tiff(&out_for_task.to_string_lossy(), &img, true)
                        })
                        .await
                        .map_err(|e| e.to_string())??;
                        Ok(out)
                    }
                    .await;
                    Message::StitchDone(Box::new(res))
                });
            }
            Message::StitchDone(res) => {
                self.status = match *res {
                    Ok(p) => format!("stitched {}", p.display()),
                    Err(e) => e,
                };
            }
            Message::LoadTrack => {
                return cosmic::task::future(async move {
                    let res: Result<Vec<Hill>, String> = async {
                        let filter = FileFilter::new("Track files")
                            .glob("*.gpx")
                            .glob("*.csv")
                            .glob("*.dat");
                        let path = pick_open("Load GPX Track", filter)
                            .await
                            .ok_or_else(|| "cancelled".to_string())?;
                        let hs = load_track_file(&path).map_err(|e| e.to_string())?;
                        Ok(hs)
                    }
                    .await;
                    Message::TrackLoaded(Box::new(res))
                });
            }
            Message::TrackLoaded(res) => match *res {
                Ok(hills) => {
                    // remap known hills by name across the index shift caused by
                    // removing the old track
                    let names: Vec<String> = self.known.iter().map(|&i| self.pan.hills[i].name.clone()).collect();
                    self.pan.remove_hills(FLAG_TRACK_POINT);
                    let n = hills.len();
                    self.pan.add_hills(&hills);
                    let base = self.pan.hills.len() - n;
                    self.track = (base..base + n).collect();
                    self.known = names
                        .iter()
                        .filter_map(|n| self.pan.get_pos(n))
                        .collect();
                    self.status = format!("track: {n} points");
                    self.bump();
                }
                Err(e) => self.status = e,
            },
            Message::ChooseViewpoint => {
                self.picker = Some(Picker {
                    title: "Choose Viewpoint".into(),
                    query: String::new(),
                    mode: PickerMode::Viewpoint,
                });
            }
            Message::ContextFindPeak(x, y) => {
                self.context = None;
                self.picker = Some(Picker {
                    title: "Find Peak".into(),
                    query: String::new(),
                    mode: PickerMode::FindPeak { img_x: x, img_y: y },
                });
            }
            Message::PickerQuery(q) => {
                if let Some(p) = &mut self.picker {
                    p.query = q;
                }
            }
            Message::PickerPick(i) => {
                if let Some(p) = self.picker.take() {
                    match p.mode {
                        PickerMode::Viewpoint => {
                            self.pan.set_viewpoint(Some(i));
                            self.sync_inputs();
                            self.bump();
                            self.status = format!("viewpoint: {}", self.pan.hills[i].name);
                        }
                        PickerMode::FindPeak { img_x, img_y } => {
                            if !self.known.contains(&i) {
                                self.known.push(i);
                            }
                            self.pan.hills[i].flags |= FLAG_VISIBLE;
                            if !self.pan.visible.contains(&i) {
                                self.pan.visible.push(i);
                            }
                            self.pan.hills[i].x = img_x;
                            self.pan.hills[i].y = img_y;
                            self.refit();
                        }
                    }
                }
            }
            Message::PickerClose => {
                self.picker = None;
                self.about = false;
                self.context = None;
            }
            Message::ContextToggleHidden(i) => {
                self.context = None;
                self.pan.hills[i].flags ^= FLAG_HIDDEN;
                self.bump();
            }
            Message::Surface(action) => {
                return cosmic::task::message(cosmic::Action::Surface(action));
            }
            Message::Redraw => {}
            Message::Center(a) => {
                self.pan.set_center_angle(a);
                self.bump();
            }
            Message::CompassStyle(s) => {
                self.compass_style = s;
                self.remeasure();
                self.bump();
            }
            Message::Pitch(a) => {
                self.pan.set_pitch_angle(a);
                self.bump();
            }
            Message::Roll(a) => {
                self.pan.set_roll_angle(a);
                self.bump();
            }
            Message::LabelFont(s) => {
                self.label_font = s;
                self.bump();
            }
            Message::Focal(f) => {
                self.set_focal_35mm(f);
                self.bump();
            }
            Message::Visibility(v) => {
                // the slider shows more visibility to the right; the core
                // stores a height/distance threshold (higher = fewer hills)
                self.pan.set_height_dist_ratio(0.11 - v);
                self.bump();
            }
            Message::TrackWidth(w) => self.track_width = w,
            Message::Lat(s) => self.lat_in = s,
            Message::Lon(s) => self.lon_in = s,
            Message::Height(s) => self.height_in = s,
            Message::ViewApply => {
                if let Ok(v) = self.lat_in.parse() {
                    self.pan.set_view_lat(v);
                }
                if let Ok(v) = self.lon_in.parse() {
                    self.pan.set_view_long(v);
                }
                if let Ok(v) = self.height_in.parse() {
                    self.pan.set_view_height(v);
                }
                self.bump();
            }
            Message::K0(s) => self.k0_in = s,
            Message::K1(s) => self.k1_in = s,
            Message::X0(s) => self.x0_in = s,
            Message::DistortionApply => {
                let k0 = self.k0_in.parse().unwrap_or(0.0);
                let k1 = self.k1_in.parse().unwrap_or(0.0);
                let x0 = self.x0_in.parse().unwrap_or(0.0);
                self.pan.set_distortion_params(k0, k1, x0);
                self.bump();
            }
            Message::ProfileName(s) => {
                self.profile_in = s;
                self.profile_overwrite = false;
            }
            Message::ProfileSave => {
                let name = self.profile_in.trim().to_string();
                if name.is_empty() {
                    self.status = "enter a profile name".into();
                } else {
                    let p = gipfel_core::profile::Profile {
                        k0: self.pan.parms.k0,
                        k1: self.pan.parms.k1,
                        x0: self.pan.parms.x0,
                    };
                    let exists = self.profiles.iter().any(|n| *n == name);
                    if exists && !self.profile_overwrite {
                        self.profile_overwrite = true;
                        self.status =
                            format!("profile '{name}' exists - press Save again to overwrite");
                    } else {
                        match gipfel_core::profile::save(&name, p, true) {
                            Ok(()) => {
                                self.status = format!("saved profile '{name}'");
                                self.profile_overwrite = false;
                                self.refresh_profiles();
                            }
                            Err(e) => self.status = format!("save failed: {e}"),
                        }
                    }
                }
            }
            Message::ProfileLoad => {
                let name = self.profile_in.trim().to_string();
                if !name.is_empty() {
                    self.apply_profile(&name);
                }
            }
            Message::ProfileSelect(i) => {
                if let Some(name) = self.profiles.get(i).cloned() {
                    self.profile_in = name.clone();
                    self.apply_profile(&name);
                }
            }
            Message::Projection(t) => {
                self.pan.set_projection(t);
                self.bump();
            }
            Message::ToggleShowHidden => {
                self.show_hidden = !self.show_hidden;
                self.bump();
            }
            Message::FitDistortion(v) => {
                self.fit_distortion = v;
                // fitting a newly unlocked calibration takes effect at once
                if v && self.known.len() >= 4 {
                    self.refit();
                }
            }
            Message::Fit => {
                self.fit = true;
                self.zoom = self.fit_zoom();
                self.panv = Vector::ZERO;
            }
            Message::ZoomIn => {
                return self.update_vp(VpMsg::ZoomAt { factor: 1.25, img: cosmic::iced::Point::ORIGIN });
            }
            Message::ZoomOut => {
                return self.update_vp(VpMsg::ZoomAt { factor: 0.8, img: cosmic::iced::Point::ORIGIN });
            }
            Message::About(b) => self.about = b,
            Message::Quit => {
                if let Some(id) = self.core.main_window_id() {
                    return cosmic::iced::window::close(id);
                }
            }
        }
        Task::none()
    }

    /// The settings panel lives in the framework-managed left nav slot
    /// (standard padding, condensed-mode handling, collapse state).
    fn nav_bar(&self) -> Option<Element<'_, cosmic::Action<Message>>> {
        if !self.core().nav_bar_active() {
            return None;
        }
        let close: Element<Message> = widget::button::icon(
            widget::icon::from_name("pan-start-symbolic").size(16),
        )
        .on_press(Message::ToggleNav)
        .into();
        let panel: Element<Message> = widget::container(
            widget::column::with_children([
                widget::row::with_children([
                    widget::text::heading("gipfel").into(),
                    widget::container(close)
                        .width(Length::Fill)
                        .align_x(cosmic::iced::alignment::Horizontal::Right)
                        .into(),
                ])
                .align_y(cosmic::iced::alignment::Vertical::Center)
                .spacing(8)
                .into(),
                widget::scrollable(self.sidebar()).into(),
            ])
            .spacing(10),
        )
        .padding([10, 12])
        .width(Length::Fixed(340.0))
        .height(Length::Fill)
        .class(cosmic::theme::Container::Secondary)
        .into();
        Some(panel.map(cosmic::Action::App))
    }

    fn view(&self) -> Element<'_, Message> {
        let content: Element<Message> = match &self.project {
            Some(p) => {
                let vp = ViewportWidget::<Message> {
                    handle: p.handle.clone(),
                    img_w: p.w as f32,
                    img_h: p.h as f32,
                    zoom: self.zoom,
                    pan: self.panv,
                    markers: self.markers(),
                    track: self.track_pts(),
                    hover: self.hover,
                    ctrl: self.ctrl,
                    font_size: self.label_font,
                    version: self.version,
                    _msg: PhantomData,
                };
                let vp: Element<Message> = Element::new(vp);
                // standard popover context menu (like cosmic-files): opens in
                // its own surface, never shifts the main content
                let vp: Element<Message> =
                    cosmic::widget::context_menu(vp, self.context_menu_tree())
                        .window_id(
                            self.core
                                .main_window_id()
                                .unwrap_or(cosmic::iced::window::Id::RESERVED),
                        )
                        .on_surface_action(Message::Surface)
                        .into();
                if self.compass_style == CompassStyle::Off {
                    vp
                } else {
                    let fov = match self.pan.projection_type {
                        ProjectionType::Rectilinear => (
                            2.0 * ((p.w as f64 * 0.5) / self.pan.parms.scale).atan()
                        )
                        .to_degrees(),
                        ProjectionType::Cylindrical => {
                            (p.w as f64 / self.pan.parms.scale).to_degrees()
                        }
                    };
                    let strip: Element<Message> = Element::new(Compass {
                        style: self.compass_style,
                        center_deg: self.pan.get_center_angle(),
                        zoom: self.zoom,
                        pan_x: self.panv.x,
                        parms: self.pan.parms,
                        ptype: self.pan.projection_type,
                        fov_deg: fov,
                        max_view_angle: self.pan.max_view_angle,
                        on_change: Rc::new(Message::Center),
                    });
                    widget::column::with_children([vp, strip]).into()
                }
            }
            None => widget::container(
                widget::column::with_children([
                    widget::text::title2("gipfel").into(),
                    widget::text("Open a JPEG to start (File ▸ Open Image…, Ctrl+O).").into(),
                    widget::button::suggested("Open Image…").on_press(Message::OpenImage).into(),
                ])
                .spacing(12)
                .align_x(cosmic::iced::alignment::Horizontal::Center),
            )
            .into(),
        };

        let main: Element<Message> = content;

        let status_text = self.hover_info.as_deref().unwrap_or(&self.status);
        let status: Element<Message> = widget::container(widget::text::body(status_text.to_string()))
            .padding([4, 10])
            .width(Length::Fill)
            .style(|_: &Theme| widget::container::Style {
                background: Some(cosmic::iced::Background::Color(Color::from_rgba(0.0, 0.0, 0.0, 0.15))),
                ..Default::default()
            })
            .into();

        let body: Element<Message> = widget::column::with_children([main, status]).into();

        if let Some(picker) = &self.picker {
            return self.with_dialog(body, self.view_picker(picker));
        }
        if self.about {
            return self.with_dialog(body, self.view_about());
        }
        body
    }
}

// helpers split out to keep view() readable

/// settings row with the label above a full-width, easy-grip slider
fn slider_item(
    label: &str,
    value: String,
    slider: impl Into<Element<'static, Message>>,
) -> Element<'static, Message> {
    let header: Element<'static, Message> = widget::row::with_children([
        widget::text::body(label.to_string()).into(),
        widget::container(widget::text::body(value))
            .width(Length::Fill)
            .align_x(cosmic::iced::alignment::Horizontal::Right)
            .into(),
    ])
    .into();
    let body: Element<'static, Message> = slider.into();
    widget::column::with_children([header, body])
        .spacing(2)
        .into()
}

/// Async file chooser: returns the chosen path, or None if cancelled.
async fn pick_open(
    title: &str,
    filter: cosmic::dialog::file_chooser::FileFilter,
) -> Option<PathBuf> {
    cosmic::dialog::file_chooser::open::Dialog::new()
        .title(title.to_string())
        .filter(filter)
        .open_file()
        .await
        .ok()?
        .url()
        .to_file_path()
        .ok()
}

/// Async save dialog: returns the target path, or None if cancelled.
async fn pick_save(
    title: &str,
    filter: cosmic::dialog::file_chooser::FileFilter,
) -> Option<PathBuf> {
    cosmic::dialog::file_chooser::save::Dialog::new()
        .title(title.to_string())
        .filter(filter)
        .save_file()
        .await
        .ok()?
        .url()
        .and_then(|u| u.to_file_path().ok())
}

impl Gipfel {
    fn with_dialog<'a>(&self, body: Element<'a, Message>, dialog: Element<'a, Message>) -> Element<'a, Message> {
        widget::column::with_children([
            widget::container(dialog).width(Length::Fill).into(),
            body,
        ])
        .into()
    }

    fn view_picker<'a>(&self, p: &'a Picker) -> Element<'a, Message> {
        let entries = self.picker_entries(p);
        let mut list = widget::column::with_capacity(entries.len());
        let mut empty = true;
        for (label, idx) in entries {
            empty = false;
            list = list.push(
                widget::button::text(label)
                    .on_press(Message::PickerPick(idx))
                    .width(Length::Fill),
            );
        }
        if empty {
            list = list.push(widget::text("no matches"));
        }

        widget::dialog()
            .title(p.title.clone())
            .body("Type to search, click to select.")
            .control(
                widget::text_input("search…", &p.query)
                    .on_input(Message::PickerQuery),
            )
            .control({
                    let el: Element<Message> = list.into();
                    widget::scrollable(el).height(Length::Fixed(320.0))
                })
            .secondary_action(widget::button::text("Cancel").on_press(Message::PickerClose))
            .is_overlay(false)
            .into()
    }

    fn context_menu_tree(&self) -> Option<Vec<menu::Tree<Message>>> {
        let ctx = self.context.as_ref()?;
        let mut items: Vec<menu::Item<CtxAction, String>> = Vec::new();
        // placing a marker only makes sense on empty image space, not on an
        // existing peak marker
        if ctx.hit.is_none() {
            items.push(menu::Item::Button(
                "Find Peak…".to_string(),
                None,
                CtxAction::FindPeak(ctx.img_x, ctx.img_y),
            ));
        }
        if let Some(i) = ctx.hit {
            let h = &self.pan.hills[i];
            items.push(menu::Item::Button(
                format!(
                    "{} {} ({} m)",
                    if h.flags & FLAG_HIDDEN != 0 { "Unhide" } else { "Hide" },
                    h.name,
                    h.height as i64
                ),
                None,
                CtxAction::ToggleHidden(i),
            ));
        }
        if items.is_empty() {
            return None;
        }
        Some(menu::items(&std::collections::HashMap::new(), items))
    }

    fn view_about<'a>(&self) -> Element<'a, Message> {
        widget::dialog()
            .title("About gipfel")
            .body("gipfel 0.5.0 (COSMIC preview)")
            .control(widget::text("Annotate mountain panoramas."))
            .control(widget::text("Port of gipfel by Johannes Hofmann - mathematical models unchanged."))            .primary_action(widget::button::suggested("Close").on_press(Message::About(false)))
            .is_overlay(false)
            .into()
    }

    fn sidebar(&self) -> Element<'_, Message> {
        let has_img = self.project.is_some();
        let focal = self.focal_35mm().clamp(1.0, 300.0);
        let pitch = self.pan.get_pitch_angle().clamp(-40.0, 20.0);
        let roll = self.pan.get_roll_angle().clamp(-10.0, 10.0);
        let vis = self.pan.height_dist_ratio.clamp(0.01, 0.1);
        let vis_ui = 0.11 - vis; // slider: right = more hills visible
        let label_font = self.label_font;
        let center = self.pan.get_center_angle().rem_euclid(360.0);

        let vp_name = self
            .pan
            .view_name
            .clone()
            .unwrap_or_else(|| "manual".to_string());

        let sec_viewpoint = widget::settings::section()
            .title("Viewpoint")
            .add(widget::settings::item(
                "Position",
                widget::row::with_children([
                    widget::text(vp_name).into(),
                    widget::button::text("Choose…").on_press(Message::ChooseViewpoint).into(),
                ])
                .spacing(8),
            ))
            .add(widget::settings::item(
                "Latitude",
                widget::text_input("°", &self.lat_in)
                    .on_input(Message::Lat)
                    .on_submit(|_| Message::ViewApply),
            ))
            .add(widget::settings::item(
                "Longitude",
                widget::text_input("°", &self.lon_in)
                    .on_input(Message::Lon)
                    .on_submit(|_| Message::ViewApply),
            ))
            .add(widget::settings::item(
                "Height (m)",
                widget::text_input("m", &self.height_in)
                    .on_input(Message::Height)
                    .on_submit(|_| Message::ViewApply),
            ));

        let sec_orientation = widget::settings::section()
            .title("Orientation")
            .add(widget::settings::item(
                "Center (deg)",
                widget::text(format!("{center:.1}°")),
            ))
            .add(slider_item(
                "Focal length (35mm)",
                format!("{focal:.1} mm"),
                widget::slider(1.0..=300.0, focal, Message::Focal)
                    .step(0.01)
                    .height(30.0),
            ))
            .add(slider_item(
                "Pitch (deg)",
                format!("{pitch:.1}°"),
                widget::slider(-40.0..=20.0, pitch, Message::Pitch)
                    .step(0.01)
                    .height(30.0),
            ))
            .add(slider_item(
                "Roll (deg)",
                format!("{roll:.1}°"),
                widget::slider(-10.0..=10.0, roll, Message::Roll)
                    .step(0.01)
                    .height(30.0),
            ));

        let sec_display = widget::settings::section()
            .title("Display")
            .add(slider_item(
                "Visibility",
                format!("{:.0}%", (vis_ui - 0.01) / 0.09 * 100.0),
                widget::slider(0.01..=0.1, vis_ui, Message::Visibility)
                    .step(0.001)
                    .height(30.0),
            ))
            .add(slider_item(
                "Track width",
                format!("{:.0} m at 1 km", self.track_width),
                widget::slider(1.0..=500.0, self.track_width, Message::TrackWidth)
                    .step(1.0)
                    .height(30.0),
            ))
            .add(slider_item(
                "Label size",
                format!("{label_font:.0} px"),
                widget::slider(8.0f32..=28.0, label_font, Message::LabelFont)
                    .step(1.0)
                    .height(30.0),
            ))
            .add(widget::settings::item(
                "Show hidden",
                widget::toggler(self.show_hidden).on_toggle(|_| Message::ToggleShowHidden),
            ))
            .add(widget::row::with_children([
                widget::button::text("Zoom −").on_press(Message::ZoomOut).into(),
                widget::button::text("Fit").on_press(Message::Fit).into(),
                widget::button::text("Zoom +").on_press(Message::ZoomIn).into(),
                widget::text(format!("{:.0}%", self.zoom * 100.0)).into(),
            ])
            .spacing(8));

        let sec_projection = widget::settings::section().title("Projection").add(
            widget::row::with_children([
                widget::radio(
                    "Normal",
                    ProjectionType::Rectilinear,
                    Some(self.pan.projection_type),
                    Message::Projection,
                )
                .into(),
                widget::radio(
                    "Panoramic",
                    ProjectionType::Cylindrical,
                    Some(self.pan.projection_type),
                    Message::Projection,
                )
                .into(),
            ])
            .spacing(16),
        );

        // distortion fitting is locked unless enough markers are placed
        let can_fit = self.known.len() >= 4;
        let fit_cb = widget::checkbox(self.fit_distortion);
        let fit_cb = if can_fit {
            fit_cb.on_toggle(Message::FitDistortion)
        } else {
            fit_cb.on_toggle_maybe(None as Option<fn(bool) -> Message>)
        };
        let mut fit_col = vec![fit_cb.into()];
        if !can_fit {
            fit_col.push(widget::text("position 4 or more markers to enable").size(11).into());
        }

        // fixed-width label column: every control starts at the same x and
        // therefore shares the same width (settings::item sizes labels to
        // their text, which made the profile field narrower than k0/k1/x0)
        let mut sec_distortion = widget::settings::section()
            .title("Distortion")
            .add(widget::settings::item_row(vec![
                widget::text("Fit distortion").width(110).into(),
                widget::column::with_children(fit_col).spacing(2).into(),
            ]))
            .add(widget::settings::item_row(vec![
                widget::text("k0").width(110).into(),
                widget::text_input("0", &self.k0_in)
                    .on_input(Message::K0)
                    .on_submit(|_| Message::DistortionApply)
                    .into(),
            ]))
            .add(widget::settings::item_row(vec![
                widget::text("k1").width(110).into(),
                widget::text_input("0", &self.k1_in)
                    .on_input(Message::K1)
                    .on_submit(|_| Message::DistortionApply)
                    .into(),
            ]))
            .add(widget::settings::item_row(vec![
                widget::text("x0").width(110).into(),
                widget::text_input("0", &self.x0_in)
                    .on_input(Message::X0)
                    .on_submit(|_| Message::DistortionApply)
                    .into(),
            ]))
            .add(widget::settings::item_row(vec![
                widget::text("profile").width(110).into(),
                widget::text_input("Camera_Model_50.00_mm", &self.profile_in)
                    .on_input(Message::ProfileName)
                    .on_submit(|_| Message::ProfileSave)
                    .into(),
            ]))
            .add(
                widget::row::with_children([
                    widget::button::suggested("Save profile")
                        .on_press(Message::ProfileSave)
                        .into(),
                    widget::button::text("Load profile")
                        .on_press(Message::ProfileLoad)
                        .into(),
                ])
                .spacing(8),
            );
        if !self.profiles.is_empty() {
            // stored profiles (shared with the C version); selecting one
            // loads its parameters
            let win = self
                .core
                .main_window_id()
                .unwrap_or(cosmic::iced::window::Id::RESERVED);
            sec_distortion = sec_distortion.add(widget::settings::item_row(vec![
                widget::text("stored").width(110).into(),
                widget::dropdown::popup_dropdown(
                    self.profiles.clone(),
                    None,
                    Message::ProfileSelect,
                    win,
                    Message::Surface,
                    |m| m,
                )
                .into(),
            ]));
        }

        let mut col = widget::column::with_children([
            sec_viewpoint.into(),
            sec_orientation.into(),
            sec_projection.into(),
            sec_distortion.into(),
            sec_display.into(),
        ])
        .spacing(12)
        .padding(12);

        if !has_img {
            col = col.push(widget::text("Load an image to enable the controls."));
        }
        col.into()
    }
}

fn main() -> cosmic::iced::Result {
    let mut flags = Flags { data: None, image: None };
    let mut args = std::env::args().skip(1);
    while let Some(a) = args.next() {
        match a.as_str() {
            "--data" => flags.data = args.next().map(PathBuf::from),
            s if !s.starts_with('-') && flags.image.is_none() => flags.image = Some(PathBuf::from(s)),
            _ => {}
        }
    }
    cosmic::app::run::<Gipfel>(
        cosmic::app::Settings::default().size(cosmic::iced::Size::new(1280.0, 800.0)),
        flags,
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn elev_inverse_matches_projection() {
        let mut pan = Panorama::new();
        pan.set_projection(ProjectionType::Rectilinear);
        pan.set_center_angle(0.0);
        pan.set_pitch_angle(0.0);
        pan.set_scale(200.0);
        // undistorted rectilinear: y = scale * tan(pitch - elev)
        for &y in &[-100.0, -30.0, 0.0, 45.0, 120.0] {
            let e = elev_at_img_y(&pan, y);
            let (_, y_back) = pan.project_point(0.0, e).unwrap();
            assert!((y_back - y).abs() < 0.01, "y={y} -> elev={e} -> {y_back}");
        }
        // with pitch the band is asymmetric but still invertible
        pan.set_pitch_angle(10.0);
        let p = pan.parms.a_pitch;
        let top = elev_at_img_y(&pan, -100.0);
        let bot = elev_at_img_y(&pan, 100.0);
        assert!(top > p && bot < p, "top={top} p={p} bot={bot}");
        let (_, y_back) = pan.project_point(0.0, top).unwrap();
        assert!((y_back + 100.0).abs() < 0.01);
    }
}
