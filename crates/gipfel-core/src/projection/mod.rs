//! Projections: image coordinates as a function of azimuth/elevation angle,
//! with symbolically pre-generated derivatives (see `codegen/`).
//!
//! Faithful port of gipfel's `ProjectionLSQ.{H,cxx}`, `ProjectionRectilinear.*`
//! and `ProjectionCylindrical.*`.

pub mod cylindrical;
pub mod rectilinear;

pub const NAN: f64 = f64::NAN;

/// The 7 fitted view parameters (radians where angular).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ViewParams {
    /// azimuth of image center, radians
    pub a_center: f64,
    /// focal scale (pixels)
    pub scale: f64,
    /// pitch angle of image center (camera nose up), radians
    pub a_pitch: f64,
    /// camera roll, radians
    pub a_roll: f64,
    /// radial distortion coefficient 0
    pub k0: f64,
    /// radial distortion coefficient 1
    pub k1: f64,
    /// distortion center x, pixels
    pub x0: f64,
}

impl ViewParams {
    pub fn new() -> Self {
        ViewParams {
            a_center: 0.0,
            scale: 3500.0,
            a_pitch: 0.0,
            a_roll: 0.0,
            k0: 0.0,
            k1: 0.0,
            x0: 0.0,
        }
    }
}

/// The 9 arguments every generated macro function takes.
#[derive(Clone, Copy, Debug)]
pub struct ProjArgs {
    pub c_view: f64,
    pub c_pitch: f64,
    pub c_roll: f64,
    pub scale: f64,
    pub k0: f64,
    pub k1: f64,
    pub x0: f64,
    pub m_view: f64,
    pub m_elev: f64,
}

impl ProjArgs {
    pub fn from_params(p: &ViewParams, m_view: f64, m_elev: f64) -> Self {
        ProjArgs {
            c_view: p.a_center,
            c_pitch: p.a_pitch,
            c_roll: p.a_roll,
            scale: p.scale,
            k0: p.k0,
            k1: p.k1,
            x0: p.x0,
            m_view,
            m_elev,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ProjectionType {
    Rectilinear = 0,
    Cylindrical = 1,
}

/// A projection plus its symbolic partial derivatives w.r.t. the view
/// parameters. Methods default to `NAN`, matching the C++ base class.
pub trait Projection: Send + Sync {
    /// half-width of the visible azimuth range, radians
    fn get_view_angle(&self) -> f64;

    fn mac_x(&self, a: ProjArgs) -> f64 {
        let _ = a;
        NAN
    }
    fn mac_y(&self, a: ProjArgs) -> f64 {
        let _ = a;
        NAN
    }
    fn mac_x_dc_view(&self, a: ProjArgs) -> f64 {
        let _ = a;
        NAN
    }
    fn mac_x_dc_pitch(&self, a: ProjArgs) -> f64 {
        let _ = a;
        NAN
    }
    fn mac_x_dc_roll(&self, a: ProjArgs) -> f64 {
        let _ = a;
        NAN
    }
    fn mac_x_dscale(&self, a: ProjArgs) -> f64 {
        let _ = a;
        NAN
    }
    fn mac_x_dk0(&self, a: ProjArgs) -> f64 {
        let _ = a;
        NAN
    }
    fn mac_x_dk1(&self, a: ProjArgs) -> f64 {
        let _ = a;
        NAN
    }
    fn mac_x_dx0(&self, a: ProjArgs) -> f64 {
        let _ = a;
        NAN
    }
    fn mac_y_dc_view(&self, a: ProjArgs) -> f64 {
        let _ = a;
        NAN
    }
    fn mac_y_dc_pitch(&self, a: ProjArgs) -> f64 {
        let _ = a;
        NAN
    }
    fn mac_y_dc_roll(&self, a: ProjArgs) -> f64 {
        let _ = a;
        NAN
    }
    fn mac_y_dscale(&self, a: ProjArgs) -> f64 {
        let _ = a;
        NAN
    }
    fn mac_y_dk0(&self, a: ProjArgs) -> f64 {
        let _ = a;
        NAN
    }
    fn mac_y_dk1(&self, a: ProjArgs) -> f64 {
        let _ = a;
        NAN
    }
    fn mac_y_dx0(&self, a: ProjArgs) -> f64 {
        let _ = a;
        NAN
    }

    /// Project azimuth/elevation to image coordinates (relative to image center),
    /// normalizing azimuth around the view center like the C++ original.
    fn get_coordinates(&self, alph: f64, elev: f64, parms: &ViewParams) -> (f64, f64) {
        let mut alph = alph;
        let pi = crate::hill::PI;

        if alph - parms.a_center > pi {
            alph -= 2.0 * pi;
        } else if alph - parms.a_center < -pi {
            alph += 2.0 * pi;
        }

        let args = ProjArgs::from_params(parms, alph, elev);
        (self.mac_x(args), self.mac_y(args))
    }
}

pub fn projection_from_type(t: ProjectionType) -> Box<dyn Projection> {
    match t {
        ProjectionType::Rectilinear => Box::new(rectilinear::ProjectionRectilinear),
        ProjectionType::Cylindrical => Box::new(cylindrical::ProjectionCylindrical),
    }
}
