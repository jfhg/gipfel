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

    /// Like [`Panorama::load_data`] but from raw bytes (no filesystem);
    /// used by the WASM build.
    pub fn load_data_from_bytes(&mut self, raw: &[u8]) -> std::io::Result<()> {
        let mut hills = crate::hill::load_from_bytes(raw)?;
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

    /// Pick `height_dist_ratio` so that the current view shows a
    /// reasonable number of peaks: the count is monotone decreasing in
    /// the ratio (a larger ratio drops the far, flat candidates first),
    /// so a binary search over the slider range [0.01, 0.10] finds the
    /// LARGEST ratio that still leaves at least `lo` visible peaks -
    /// the sparsest view that is not empty. If even that ratio shows
    /// more than `hi` peaks (dense alpine terrain, panoramic view) the
    /// overshoot is accepted; the user can always tighten the slider.
    /// Returns the resulting visible count.
    pub fn auto_visibility(&mut self, lo: usize, hi: usize) -> usize {
        let probe = |p: &mut Self, r: f64| -> usize {
            p.height_dist_ratio = r;
            p.update_close_mountains();
            p.update_visible_mountains(None);
            p.visible.len()
        };
        // already reasonable where it is: keep the user's/loaded state
        let now = probe(self, self.height_dist_ratio);
        if now >= lo && now <= hi {
            return now;
        }
        // binary search: largest ratio with count >= lo
        let (mut a, mut b) = (0.01_f64, 0.10_f64);
        if probe(self, a) < lo {
            // flat terrain: even the most generous ratio stays below lo
            return probe(self, a);
        }
        let mut best = a;
        while b - a > 0.002 {
            let m = 0.5 * (a + b);
            if probe(self, m) >= lo {
                best = m;
                a = m;
            } else {
                b = m;
            }
        }
        probe(self, best)
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
        // NaN or infinite azimuths (from user input or a diverged fit) must
        // never count as visible; a NaN center used to make everything so.
        if !alph.is_finite() || !self.parms.a_center.is_finite() {
            return false;
        }
        // Shortest arc to the view center. wrap_angle also covers azimuths
        // far outside [-pi, pi] (user enters "400 deg", a fit leaves it
        // unnormalized); the old single fold turned |alph - a_center| > 2pi
        // into a NEGATIVE distance that passed every comparison and made
        // hills behind the camera visible (GipfelFind.lean Finding 1).
        let center_dist = crate::projection::wrap_angle(alph - self.parms.a_center).abs();
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
            // restricted mode (-e, a curated list such as the photo
            // index): export every listed entry inside the field of
            // view. The prominence/occlusion heuristics are meant to
            // thin out the full 21k peak database, not to second-guess
            // an explicit list.
            if m.flags & crate::hill::FLAG_DUPLIC != 0
                || (!require_export && m.flags & crate::hill::FLAG_HIDDEN != 0)
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

    #[test]
    fn out_of_range_center_angle_stays_safe() {
        // GipfelFind.lean Finding 1: with |alph - a_center| > 2pi the single
        // fold produced a negative "distance" that passed every comparison.
        // Hill azimuths come from atan2, so alph is always in [-pi, pi].
        let mut raw = Panorama::new();
        raw.parms.a_center = 400f64.to_radians(); // e.g. left by a fit
        assert!(!raw.is_visible(-std::f64::consts::PI)); // 220 deg off, behind

        // the setter must agree with the equivalent in-range azimuth
        let mut pan400 = Panorama::new();
        pan400.set_center_angle(400.0); // "bearing + full turn" input slip
        let mut pan40 = Panorama::new();
        pan40.set_center_angle(40.0);
        for d in -180..=180 {
            let a = (d as f64).to_radians();
            assert_eq!(pan400.is_visible(a), pan40.is_visible(a), "az {d}");
        }

        // NaN / infinite azimuths are not visible (Finding 1b)
        assert!(!pan40.is_visible(f64::NAN));
        assert!(!pan40.is_visible(f64::INFINITY));
        assert!(!pan40.is_visible(f64::NEG_INFINITY));

        // a NaN camera azimuth hides everything (was: everything visible)
        let mut bad = Panorama::new();
        bad.parms.a_center = f64::NAN;
        assert!(!bad.is_visible(0.0));
    }

    /// Synthetic terrain: `near` peaks ~10 km away and `far` hills
    /// ~120 km away, all inside the +-60 deg default view. Hill
    /// positions are geocentric radians (as after load_data); the
    /// close-candidate ratio is height/distance_m.
    fn terrain(near: usize, far: usize) -> Panorama {
        let mut pan = Panorama::new();
        let mut hills = Vec::new();
        let d_near = 10_000.0 / 6_371_000.0; // 10 km in radians
        let d_far = 120_000.0 / 6_371_000.0; // 120 km in radians
        for k in 0..near {
            let az = (-40.0 + k as f64 * 80.0 / (near.max(2) - 1) as f64).to_radians();
            // 500 m at 10 km -> ratio 0.05: below the default 0.07,
            // inside the slider range
            hills.push(Hill::new(
                &format!("near{k}"),
                d_near * az.cos(),
                d_near * az.sin(),
                500.0,
            ));
        }
        for k in 0..far {
            let az = (-80.0 + k as f64 * 160.0 / (far.max(2) - 1) as f64).to_radians();
            // 800 m at 120 km -> ratio 0.0067: below the slider floor
            hills.push(Hill::new(
                &format!("far{k}"),
                d_far * az.cos(),
                d_far * az.sin(),
                800.0,
            ));
        }
        pan.add_hills(&hills);
        pan
    }

    #[test]
    fn auto_visibility_targets_reasonable_count() {
        // default ratio 0.07 shows nothing (near peaks are 0.05) ->
        // search must find the band that keeps the 15 near peaks and
        // drops the 300 far ones
        let mut pan = terrain(15, 300);
        let n = pan.auto_visibility(8, 40);
        assert!((8..=40).contains(&n), "count {n}");
        assert!(pan.height_dist_ratio > 0.01 && pan.height_dist_ratio < 0.06);
    }

    #[test]
    fn auto_visibility_flat_terrain_shows_everything() {
        let mut pan = terrain(0, 300);
        let n = pan.auto_visibility(8, 40);
        assert!(n < 8);
        assert!((pan.height_dist_ratio - 0.01).abs() < 1e-9);
    }

    #[test]
    fn auto_visibility_dense_terrain_accepts_overshoot() {
        // 200 near peaks: no ratio in the slider range lands inside
        // [8, 40]; the sparsest setting is chosen and the overshoot
        // accepted
        let mut pan = terrain(200, 0);
        let n = pan.auto_visibility(8, 40);
        assert!(n > 40);
        assert!(pan.height_dist_ratio > 0.03);
    }

    #[test]
    fn auto_visibility_keeps_good_state() {
        // 20 near peaks (ratio 0.05) at r=0.02: already in range, the
        // search must not move the ratio
        let mut pan = terrain(20, 0);
        pan.set_height_dist_ratio(0.02);
        let r0 = pan.height_dist_ratio;
        let n = pan.auto_visibility(8, 40);
        assert_eq!(n, 20);
        assert_eq!(pan.height_dist_ratio, r0);
    }
}
