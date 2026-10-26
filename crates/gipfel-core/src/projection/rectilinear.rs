//! Rectilinear (gnomonic) projection with lens distortion.

mod funcs {
    #![allow(non_snake_case, unused_parens, unused_variables, dead_code, clippy::all)]

    // math shims: the maxima-generated code uses C-style calls
    #[inline(always)]
    fn sin(x: f64) -> f64 {
        x.sin()
    }
    #[inline(always)]
    fn cos(x: f64) -> f64 {
        x.cos()
    }
    #[inline(always)]
    fn tan(x: f64) -> f64 {
        x.tan()
    }
    #[inline(always)]
    fn sqrt(x: f64) -> f64 {
        x.sqrt()
    }
    #[inline(always)]
    fn pow(x: f64, y: f64) -> f64 {
        x.powf(y)
    }
    #[inline(always)]
    fn exp(x: f64) -> f64 {
        x.exp()
    }
    #[inline(always)]
    fn fabs(x: f64) -> f64 {
        x.abs()
    }
    #[inline(always)]
    fn atan2(y: f64, x: f64) -> f64 {
        y.atan2(x)
    }
    #[inline(always)]
    fn sec(x: f64) -> f64 {
        1.0 / x.cos()
    }

    include!("generated/rectilinear_funcs.rs");
}

use super::{ProjArgs, Projection};

pub struct ProjectionRectilinear;

impl Projection for ProjectionRectilinear {
    fn get_view_angle(&self) -> f64 {
        1.0471976 /* 60 deg */
    }

    fn mac_x(&self, a: ProjArgs) -> f64 {
        funcs::mac_x(a.c_view, a.c_pitch, a.c_roll, a.scale, a.k0, a.k1, a.x0, a.m_view, a.m_elev)
    }
    fn mac_y(&self, a: ProjArgs) -> f64 {
        funcs::mac_y(a.c_view, a.c_pitch, a.c_roll, a.scale, a.k0, a.k1, a.x0, a.m_view, a.m_elev)
    }
    fn mac_x_dc_view(&self, a: ProjArgs) -> f64 {
        funcs::mac_x_dc_view(a.c_view, a.c_pitch, a.c_roll, a.scale, a.k0, a.k1, a.x0, a.m_view, a.m_elev)
    }
    fn mac_x_dc_pitch(&self, a: ProjArgs) -> f64 {
        funcs::mac_x_dc_pitch(a.c_view, a.c_pitch, a.c_roll, a.scale, a.k0, a.k1, a.x0, a.m_view, a.m_elev)
    }
    fn mac_x_dc_roll(&self, a: ProjArgs) -> f64 {
        funcs::mac_x_dc_roll(a.c_view, a.c_pitch, a.c_roll, a.scale, a.k0, a.k1, a.x0, a.m_view, a.m_elev)
    }
    fn mac_x_dscale(&self, a: ProjArgs) -> f64 {
        funcs::mac_x_dscale(a.c_view, a.c_pitch, a.c_roll, a.scale, a.k0, a.k1, a.x0, a.m_view, a.m_elev)
    }
    fn mac_x_dk0(&self, a: ProjArgs) -> f64 {
        funcs::mac_x_dk0(a.c_view, a.c_pitch, a.c_roll, a.scale, a.k0, a.k1, a.x0, a.m_view, a.m_elev)
    }
    fn mac_x_dk1(&self, a: ProjArgs) -> f64 {
        funcs::mac_x_dk1(a.c_view, a.c_pitch, a.c_roll, a.scale, a.k0, a.k1, a.x0, a.m_view, a.m_elev)
    }
    fn mac_x_dx0(&self, a: ProjArgs) -> f64 {
        funcs::mac_x_dx0(a.c_view, a.c_pitch, a.c_roll, a.scale, a.k0, a.k1, a.x0, a.m_view, a.m_elev)
    }
    fn mac_y_dc_view(&self, a: ProjArgs) -> f64 {
        funcs::mac_y_dc_view(a.c_view, a.c_pitch, a.c_roll, a.scale, a.k0, a.k1, a.x0, a.m_view, a.m_elev)
    }
    fn mac_y_dc_pitch(&self, a: ProjArgs) -> f64 {
        funcs::mac_y_dc_pitch(a.c_view, a.c_pitch, a.c_roll, a.scale, a.k0, a.k1, a.x0, a.m_view, a.m_elev)
    }
    fn mac_y_dc_roll(&self, a: ProjArgs) -> f64 {
        funcs::mac_y_dc_roll(a.c_view, a.c_pitch, a.c_roll, a.scale, a.k0, a.k1, a.x0, a.m_view, a.m_elev)
    }
    fn mac_y_dscale(&self, a: ProjArgs) -> f64 {
        funcs::mac_y_dscale(a.c_view, a.c_pitch, a.c_roll, a.scale, a.k0, a.k1, a.x0, a.m_view, a.m_elev)
    }
    fn mac_y_dk0(&self, a: ProjArgs) -> f64 {
        funcs::mac_y_dk0(a.c_view, a.c_pitch, a.c_roll, a.scale, a.k0, a.k1, a.x0, a.m_view, a.m_elev)
    }
    fn mac_y_dk1(&self, a: ProjArgs) -> f64 {
        funcs::mac_y_dk1(a.c_view, a.c_pitch, a.c_roll, a.scale, a.k0, a.k1, a.x0, a.m_view, a.m_elev)
    }
    fn mac_y_dx0(&self, a: ProjArgs) -> f64 {
        funcs::mac_y_dx0(a.c_view, a.c_pitch, a.c_roll, a.scale, a.k0, a.k1, a.x0, a.m_view, a.m_elev)
    }
}
