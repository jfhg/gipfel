//! Panorama: viewpoint, hill visibility (occlusion, refraction) and the
//! link between geographic coordinates and image coordinates.
//!
//! Faithful port of gipfel's `Panorama.cxx`.

use crate::hill::{Hill, DEG2RAD, PI};
use crate::projection::{projection_from_type, Projection, ProjectionType, ViewParams};

const EARTH_RADIUS: f64 = 6371000.785;

pub struct Panorama {
    pub hills: Vec<Hill>,
    /// indices into `hills`, sorted by alpha descending (rebuilt by update_angles)
    pub alpha_order: Vec<usize>,
    /// indices into `hills`, sorted by alpha descending
    pub close: Vec<usize>,
    /// indices into `hills`
    pub visible: Vec<usize>,

    pub view_name: Option<String>,
    /// latitude, radians
    pub view_phi: f64,
    /// longitude, radians
    pub view_lam: f64,
    /// meters
    pub view_height: f64,

    pub height_dist_ratio: f64,
    pub hide_value: f64,
    /// Additional visibility limit: maximum angular distance (radians) from
    /// the image center. The app sets this to the angle the projection maps
    /// to the image edge (the lens' actual field of view, including fitted
    /// distortion), so that directions outside the photo — which a distortion
    /// model can fold back into the image — neither get markers nor compass
    /// ticks. 0 = no additional limit (only the projection's own limit).
    pub max_view_angle: f64,

    pub parms: ViewParams,
    pub projection_type: ProjectionType,
    proj: Box<dyn Projection>,
}

impl Default for Panorama {
    fn default() -> Self {
        Self::new()
    }
}

impl Panorama {
    pub fn new() -> Self {
        Panorama {
            hills: Vec::new(),
            alpha_order: Vec::new(),
            close: Vec::new(),
            visible: Vec::new(),
            view_name: None,
            view_phi: 0.0,
            view_lam: 0.0,
            view_height: 0.0,
            height_dist_ratio: 0.07,
            hide_value: 1.2,
            max_view_angle: 0.0,
            parms: ViewParams::new(),
            projection_type: ProjectionType::Rectilinear,
            proj: projection_from_type(ProjectionType::Rectilinear),
        }
    }

    pub fn load_data(&mut self, path: &str) -> std::io::Result<()> {
        let mut hills = crate::hill::load(path)?;
        crate::hill::mark_duplicates(&mut hills, 0.00001);
        self.hills = hills;
        self.update_angles();
        Ok(())
    }

    pub fn add_hills(&mut self, extra: &[Hill]) {
        self.hills.extend_from_slice(extra);
        crate::hill::mark_duplicates(&mut self.hills, 0.00001);
        self.update_angles();
    }

    pub fn remove_hills(&mut self, flags: u32) {
        self.hills.retain(|m| m.flags & flags == 0);
        // indices shifted; rebuild all derived lists
        self.alpha_order.clear();
        self.close.clear();
        self.visible.clear();
    }

    pub fn set_viewpoint(&mut self, idx: Option<usize>) {
        let Some(idx) = idx else { return };
        let m = &self.hills[idx];
        self.view_phi = m.phi;
        self.view_lam = m.lam;
        self.view_height = m.height;
        self.view_name = Some(m.name.clone());
        self.update_angles();
    }

    /// Fit view parameters to the known hills (indices into `self.hills`).
    /// `fit_distortion` enables the 7-parameter fit (k0/k1/x0) for 4+ hills;
    /// otherwise the distortion params stay locked at their current values.
    pub fn comp_params(&mut self, known: &[usize], fit_distortion: bool) -> Result<(), String> {
        let ret = crate::lsq::comp_params(&*self.proj, &self.hills, known, &mut self.parms, fit_distortion);
        if ret.is_ok() {
            self.update_visible_mountains(Some(known));
        }
        ret
    }

    /// Like [`Panorama::comp_params`] but the observed hills are an external
    /// slice (used by the golden fit tests, mirroring the C harness).
    pub fn comp_params_with(&mut self, hills: &[Hill], known: &[usize], fit_distortion: bool) -> Result<(), String> {
        let ret = crate::lsq::comp_params(&*self.proj, hills, known, &mut self.parms, fit_distortion);
        if ret.is_ok() {
            self.update_visible_mountains(None);
        }
        ret
    }

    pub fn set_center_angle(&mut self, deg: f64) {
        self.parms.a_center = deg * DEG2RAD;
        self.update_visible_mountains(None);
    }

    pub fn set_pitch_angle(&mut self, deg: f64) {
        self.parms.a_pitch = deg * DEG2RAD;
        self.update_coordinates(None);
    }

    pub fn set_roll_angle(&mut self, deg: f64) {
        self.parms.a_roll = deg * DEG2RAD;
        self.update_coordinates(None);
    }

    pub fn set_scale(&mut self, s: f64) {
        self.parms.scale = s;
        self.update_coordinates(None);
    }

    pub fn set_distortion_params(&mut self, k0: f64, k1: f64, x0: f64) {
        self.parms.k0 = k0;
        self.parms.k1 = k1;
        self.parms.x0 = x0;
        self.update_coordinates(None);
    }

    pub fn set_height_dist_ratio(&mut self, r: f64) {
        self.height_dist_ratio = r;
        self.update_close_mountains();
    }

    pub fn set_view_long(&mut self, deg: f64) {
        self.view_lam = deg * DEG2RAD;
        self.update_angles();
    }

    pub fn set_view_lat(&mut self, deg: f64) {
        self.view_phi = deg * DEG2RAD;
        self.update_angles();
    }

    pub fn set_view_height(&mut self, v: f64) {
        self.view_height = v;
        self.update_angles();
    }

    pub fn set_projection(&mut self, t: ProjectionType) {
        self.projection_type = t;
        self.proj = projection_from_type(t);
        self.update_angles();
    }

    pub fn get_center_angle(&self) -> f64 {
        self.parms.a_center / DEG2RAD
    }
    pub fn get_pitch_angle(&self) -> f64 {
        self.parms.a_pitch / DEG2RAD
    }
    pub fn get_roll_angle(&self) -> f64 {
        self.parms.a_roll / DEG2RAD
    }
    pub fn get_scale(&self) -> f64 {
        self.parms.scale
    }
    pub fn get_view_long(&self) -> f64 {
        self.view_lam / DEG2RAD
    }
    pub fn get_view_lat(&self) -> f64 {
        self.view_phi / DEG2RAD
    }
    pub fn get_view_height(&self) -> f64 {
        self.view_height
    }

    /// Last hill whose name matches exactly (C keeps the last match).
    pub fn get_pos(&self, name: &str) -> Option<usize> {
        let mut ret = None;
        for (i, m) in self.hills.iter().enumerate() {
            if m.name == name {
                ret = Some(i);
            }
        }
        ret
    }

    pub fn update_angles(&mut self) {
        for i in 0..self.hills.len() {
            let (phi, lam) = (self.hills[i].phi, self.hills[i].lam);
            let d = self.distance(phi, lam);
            self.hills[i].dist = d;
            if phi != self.view_phi || lam != self.view_lam {
                let a = self.alpha(i);
                self.hills[i].alph = a;
            }
        }

        // keep `hills` in insertion order (indices stay stable) and maintain a
        // separate alpha-descending order, mirroring the C pointer-array sort
        let mut idx: Vec<usize> = (0..self.hills.len()).collect();
        idx.sort_by(|&a, &b| {
            self.hills[b]
                .alph
                .partial_cmp(&self.hills[a].alph)
                .unwrap_or(std::cmp::Ordering::Equal)
        });
        self.alpha_order = idx;

        self.update_close_mountains();
    }

    /// Mark hills that are hidden behind a nearer, taller neighbour.
    /// Faithful port including the original's `fabs(m->alph - n->alph > pi/2)`
    /// oddity (comparison inside fabs, i.e. skip when m.alph - n.alph > pi/2).
    pub fn mark_hidden(&mut self, hills: &[usize]) {
        for i in 0..hills.len() {
            let m = hills[i];
            self.hills[m].flags &= !crate::hill::FLAG_HIDDEN;

            if self.hills[m].flags & crate::hill::FLAG_DUPLIC != 0 {
                continue;
            }

            for j in 0..hills.len() {
                let n = hills[j];
                if self.hills[n].flags & crate::hill::FLAG_DUPLIC != 0
                    || self.hills[n].flags & crate::hill::FLAG_TRACK_POINT != 0
                {
                    continue;
                }
                if n == m || self.hills[m].alph - self.hills[n].alph > PI / 2.0 {
                    continue;
                }
                if self.hills[m].dist < self.hills[n].dist
                    || self.hills[m].a_elev > self.hills[n].a_elev
                {
                    continue;
                }

                let h = (self.hills[n].a_elev - self.hills[m].a_elev)
                    / (self.hills[m].alph - self.hills[n].alph).abs();
                if h.is_infinite() || h > self.hide_value {
                    self.hills[m].flags |= crate::hill::FLAG_HIDDEN;
                }
            }
        }
    }

    pub fn update_close_mountains(&mut self) {
        self.close.clear();

        let order = self.alpha_order.clone();
        for &i in &order {
            let m = &self.hills[i];
            if m.flags & crate::hill::FLAG_TRACK_POINT != 0
                || ((m.phi != self.view_phi || m.lam != self.view_lam)
                    && (m.height / (m.dist * EARTH_RADIUS) > self.height_dist_ratio))
            {
                let elev = self.elevation(i);
                self.hills[i].a_elev = elev;
                self.close.push(i);
            }
        }

        let close = self.close.clone();
        self.mark_hidden(&close);
        self.update_visible_mountains(None);
    }

    pub fn update_visible_mountains(&mut self, excluded: Option<&[usize]>) {
        self.visible.clear();

        for k in 0..self.close.len() {
            let i = self.close[k];
            if self.is_visible(self.hills[i].alph) {
                self.visible.push(i);
                self.hills[i].flags |= crate::hill::FLAG_VISIBLE;
            } else {
                self.hills[i].flags &= !crate::hill::FLAG_VISIBLE;
            }
        }

        let owned;
        let excl: Option<&[usize]> = match excluded {
            Some(e) => {
                owned = e.to_vec();
                Some(&owned)
            }
            None => None,
        };
        self.update_coordinates(excl);
    }

    pub fn update_coordinates(&mut self, excluded: Option<&[usize]>) {
        for k in 0..self.visible.len() {
            let i = self.visible[k];
            if let Some(e) = excluded {
                if e.contains(&i) {
                    continue;
                }
            }
            let (alph, elev) = (self.hills[i].alph, self.hills[i].a_elev);
            let (x, y) = self.proj.get_coordinates(alph, elev, &self.parms);
            self.hills[i].x = x;
            self.hills[i].y = y;
        }
    }

    /// angular distance (fraction of earth radius) viewpoint -> (phi, lam)
    pub fn distance(&self, phi: f64, lam: f64) -> f64 {
        let d_lam = self.view_lam - lam;

        let t1 = (phi.cos() * d_lam.sin()).powi(2);
        let t2 = (self.view_phi.cos() * phi.sin()
            - self.view_phi.sin() * phi.cos() * d_lam.cos())
        .powi(2);

        (t1 + t2).sqrt().atan2(
            self.view_phi.sin() * phi.sin() + self.view_phi.cos() * phi.cos() * d_lam.cos(),
        )
    }

    pub fn alpha(&self, idx: usize) -> f64 {
        let m = &self.hills[idx];
        let sin_alph = (m.lam - self.view_lam).sin() * m.phi.cos() / m.dist.sin();
        let cos_alph =
            (m.phi.sin() - self.view_phi.sin() * m.dist.cos()) / (self.view_phi.cos() * m.dist.sin());

        (sin_alph.atan2(cos_alph) + 2.0 * PI) % (2.0 * PI)
    }

    /// approximation of refraction effect as described by Tom Chester at
    /// <http://tchester.org/sgm/analysis/peaks/refraction.html>
    pub fn refraction(&self, idx: usize) -> f64 {
        let (alpha, t0) = (6.5, 10.0);

        let a = 2.9e-4 * (-self.view_height / 10000.0).exp() / (1.0 + 2.9 * t0 / 760.0);
        let b = 2.9 * alpha / (760.0 * (1.0 + 2.9 * t0 / 760.0));
        let c = a * (b - 1.0 / 10.0);

        c * self.get_real_distance(idx) / (2000.0 * (1.0 + a))
    }

    pub fn elevation(&self, idx: usize) -> f64 {
        let m = &self.hills[idx];
        let theta = self.refraction(idx);

        let b = m.height + self.get_earth_radius(m.phi);
        let c = self.view_height + self.get_earth_radius(self.view_phi);

        ((m.dist.cos() * b - c) / (m.dist.sin() * b)).atan() - theta
    }

    /// local distance to center of WGS84 ellipsoid
    pub fn get_earth_radius(&self, phi: f64) -> f64 {
        let a = 6378137.000f64;
        let b = 6356752.315f64;
        let ata = phi.tan();

        a * (ata.powi(2) + 1.0).powf(1.0 / 2.0)
            * b.abs()
            * (b.powi(2) + a.powi(2) * ata.powi(2)).powf(-1.0 / 2.0)
    }

    /// real (line-of-sight) distance in meters
    pub fn get_real_distance(&self, idx: usize) -> f64 {
        let m = &self.hills[idx];
        let a = self.view_height + self.get_earth_radius(self.view_phi);
        let b = m.height + self.get_earth_radius(m.phi);
        let gam = m.dist;

        (a.powi(2) + b.powi(2) - 2.0 * a * b * gam.cos()).sqrt()
    }

    pub fn is_visible(&self, alph: f64) -> bool {
        let mut center_dist = (alph - self.parms.a_center).abs();
        if center_dist > PI {
            center_dist = 2.0 * PI - center_dist;
        }
        if center_dist >= self.proj.get_view_angle() {
            return false;
        }
        self.max_view_angle <= 0.0 || center_dist < self.max_view_angle
    }

    /// Forward-project (azimuth, elevation) to image-centered pixel coords.
    /// `None` when the direction is not visible or the projection is
    /// undefined there (mirrors C `Panorama::get_coordinates` returning 1,
    /// plus the NaN guard of C `ScanImage::get_pixel_nearest`).
    pub fn project_point(&self, alph: f64, elev: f64) -> Option<(f64, f64)> {
        if !self.is_visible(alph) {
            return None;
        }
        let (x, y) = self.proj.get_coordinates(alph, elev, &self.parms);
        if x.is_nan() || y.is_nan() {
            return None;
        }
        Some((x, y))
    }

    /// Parameter-only snapshot (no hill data) for resampling: same viewpoint
    /// and projection parameters, but without the UI's lens-FOV visibility
    /// limit (the stitch output range defines the FOV itself).
    pub fn params_snapshot(&self) -> Panorama {
        let mut p = Panorama::new();
        p.projection_type = self.projection_type;
        p.proj = projection_from_type(self.projection_type);
        p.parms = self.parms;
        p.view_phi = self.view_phi;
        p.view_lam = self.view_lam;
        p.view_height = self.view_height;
        p
    }

    pub fn get_coordinates(&self, alph: f64, elev: f64) -> Option<(f64, f64)> {
        if self.is_visible(alph) {
            Some(self.proj.get_coordinates(alph, elev, &self.parms))
        } else {
            None
        }
    }

    /// Export rows for the visible hills, mirroring the C `export_hills`
    /// output loop: `name\theight\tx\ty\tdistance` with image-center origin.
    /// When `require_export` is set, only hills with `FLAG_EXPORT` are listed.
    pub fn export_hill_rows(&self, img_w: i64, img_h: i64, require_export: bool) -> Vec<String> {
        let mut rows = Vec::new();
        for &i in &self.visible {
            let m = &self.hills[i];
            if m.flags & crate::hill::FLAG_DUPLIC != 0
                || m.flags & crate::hill::FLAG_HIDDEN != 0
                || (require_export && m.flags & crate::hill::FLAG_EXPORT == 0)
            {
                continue;
            }

            let x = m.x.round_ties_even() as i64 + img_w / 2;
            let y = m.y.round_ties_even() as i64 + img_h / 2;
            if x < 0 || x > img_w || y < 0 || y > img_h {
                continue;
            }

            rows.push(format!(
                "{}\t{}\t{}\t{}\t{}",
                m.name,
                m.height.round_ties_even() as i64,
                x,
                y,
                self.get_real_distance(i).round_ties_even() as i64
            ));
        }
        rows
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn max_view_angle_limits_visibility() {
        let mut pan = Panorama::new();
        pan.set_center_angle(0.0);
        let a30 = 30f64.to_radians();
        let a70 = 70f64.to_radians();

        // rectilinear default limit is +-60 deg
        assert!(pan.is_visible(a30));
        assert!(!pan.is_visible(a70));

        // a tighter lens FOV (say +-35 deg) hides the 40 deg direction that
        // the projection's own +-60 deg limit would allow
        pan.max_view_angle = 35f64.to_radians();
        assert!(pan.is_visible(a30));
        assert!(!pan.is_visible(40f64.to_radians()));

        // a wider limit never overrides the projection's own bound
        pan.max_view_angle = 80f64.to_radians();
        assert!(!pan.is_visible(a70));
    }
}
