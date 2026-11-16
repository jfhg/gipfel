//! Non-linear least squares fit of the view parameters.
//!
//! Faithful port of `ProjectionLSQ.cxx`. The C++ original uses GSL's
//! `gsl_multifit_fdfsolver_lmsder`; this is a self-contained Levenberg-
//! Marquardt with the same scaled damping (`JᵀJ + λ·diag(JᵀJ)`) and the
//! analytic Jacobian produced by the maxima-generated derivatives. For the
//! well-conditioned gipfel fit (residuals reach zero at the optimum) it
//! converges to the same minimum; the golden tests verify agreement with the
//! C/GSL output.

use crate::hill::Hill;
use crate::projection::{Projection, ViewParams};

const MAX_ITER: usize = 200;

/// Number of fitted parameters at a given level.
fn np_for_level(level: usize) -> usize {
    match level {
        0 => 2,
        1 => 4,
        _ => 7,
    }
}

struct FitData<'a> {
    proj: &'a dyn Projection,
    hills: &'a [Hill],
    known: &'a [usize],
    level: usize,
    /// parameters at solver start; entries above `level` keep these values
    parms0: ViewParams,
}

impl FitData<'_> {
    fn params_from_x(&self, x: &[f64]) -> ViewParams {
        // C starts from the initial params and only overwrites the entries
        // that are actually fitted at this level.
        let mut p = self.parms0;
        p.a_center = x[0];
        p.a_pitch = x[1];
        if self.level > 0 {
            p.a_roll = x[2];
            p.scale = x[3];
        }
        if self.level > 1 {
            p.k0 = x[4];
            p.k1 = x[5];
            p.x0 = x[6];
        }
        p
    }

    fn args(&self, p: &ViewParams, i: usize) -> crate::projection::ProjArgs {
        let m = &self.hills[self.known[i]];
        crate::projection::ProjArgs::from_params(p, m.alph, m.a_elev)
    }

    /// Residual vector: 2 entries (x,y) per known hill, length `2*known.len()`.
    fn residuals(&self, x: &[f64], f: &mut [f64]) {
        let p = self.params_from_x(x);
        for i in 0..self.known.len() {
            let m = &self.hills[self.known[i]];
            let a = self.args(&p, i);
            f[2 * i] = self.proj.mac_x(a) - m.x;
            f[2 * i + 1] = self.proj.mac_y(a) - m.y;
        }
    }

    /// Jacobian, row-major `m`×`np` (`m = 2*known.len()`).
    fn jacobian(&self, x: &[f64], j: &mut [f64]) {
        let p = self.params_from_x(x);
        let np = np_for_level(self.level);
        let row = |r: usize, c: usize, v: f64, j: &mut [f64]| j[r * np + c] = v;
        for i in 0..self.known.len() {
            let a = self.args(&p, i);

            row(2 * i, 0, self.proj.mac_x_dc_view(a), j);
            row(2 * i + 1, 0, self.proj.mac_y_dc_view(a), j);
            row(2 * i, 1, self.proj.mac_x_dc_pitch(a), j);
            row(2 * i + 1, 1, self.proj.mac_y_dc_pitch(a), j);

            if np > 2 {
                row(2 * i, 2, self.proj.mac_x_dc_roll(a), j);
                row(2 * i + 1, 2, self.proj.mac_y_dc_roll(a), j);
                row(2 * i, 3, self.proj.mac_x_dscale(a), j);
                row(2 * i + 1, 3, self.proj.mac_y_dscale(a), j);
            }

            if np > 4 {
                row(2 * i, 4, self.proj.mac_x_dk0(a), j);
                row(2 * i + 1, 4, self.proj.mac_y_dk0(a), j);
                row(2 * i, 5, self.proj.mac_x_dk1(a), j);
                row(2 * i + 1, 5, self.proj.mac_y_dk1(a), j);
                row(2 * i, 6, self.proj.mac_x_dx0(a), j);
                row(2 * i + 1, 6, self.proj.mac_y_dx0(a), j);
            }
        }
    }
}

fn dot(a: &[f64], b: &[f64]) -> f64 {
    a.iter().zip(b).map(|(x, y)| x * y).sum()
}

/// Solve `A·δ = b` in place (A row-major `n`×`n`, overwritten) by Gaussian
/// elimination with partial pivoting. Returns `None` if the matrix is
/// (numerically) singular.
fn gauss_solve(a: &mut [f64], b: &[f64], n: usize) -> Option<Vec<f64>> {
    let mut x = b.to_vec();
    for col in 0..n {
        // partial pivot
        let mut piv = col;
        let mut maxv = a[col * n + col].abs();
        for r in (col + 1)..n {
            let v = a[r * n + col].abs();
            if v > maxv {
                maxv = v;
                piv = r;
            }
        }
        if maxv < 1e-300 {
            return None;
        }
        if piv != col {
            for c in 0..n {
                a.swap(col * n + c, piv * n + c);
            }
            x.swap(col, piv);
        }
        let diag = a[col * n + col];
        for r in (col + 1)..n {
            let factor = a[r * n + col] / diag;
            a[r * n + col] = 0.0;
            for c in (col + 1)..n {
                a[r * n + c] -= factor * a[col * n + c];
            }
            x[r] -= factor * x[col];
        }
    }
    for r in (0..n).rev() {
        let mut s = x[r];
        for c in (r + 1)..n {
            s -= a[r * n + c] * x[c];
        }
        x[r] = s / a[r * n + r];
    }
    Some(x)
}

/// One Levenberg-Marquardt fit at a single parameter level. On return, `x`
/// holds the fitted parameters (length `np`). Returns the final cost
/// (half the squared residual sum) so callers can compare restarts.
fn lm(data: &FitData, x: &mut [f64], np: usize) -> f64 {
    let m = 2 * data.known.len();
    let mut f = vec![0.0f64; m];
    let mut fnew = vec![0.0f64; m];
    let mut jac = vec![0.0f64; m * np];
    let mut a = vec![0.0f64; np * np];
    let mut ad = vec![0.0f64; np * np];
    let mut g = vec![0.0f64; np];
    let mut neg_g = vec![0.0f64; np];
    let mut xnew = vec![0.0f64; np];

    data.residuals(x, &mut f);
    let mut cost = 0.5 * dot(&f, &f);
    let mut lambda: f64 = 1e-3;

    for _ in 0..MAX_ITER {
        if !cost.is_finite() {
            break;
        }
        data.jacobian(x, &mut jac);

        // A = JᵀJ (symmetric), g = Jᵀf
        for i in 0..np {
            for k in i..np {
                let mut s = 0.0;
                for r in 0..m {
                    s += jac[r * np + i] * jac[r * np + k];
                }
                a[i * np + k] = s;
                a[k * np + i] = s;
            }
            let mut s = 0.0;
            for r in 0..m {
                s += jac[r * np + i] * f[r];
            }
            g[i] = s;
        }

        // Solve the damped normal equations (JᵀJ + λ·diag(JᵀJ))·δ = -Jᵀf,
        // inflating λ until the system is non-singular.
        let mut delta: Option<Vec<f64>> = None;
        for _ in 0..50 {
            ad.copy_from_slice(&a);
            for i in 0..np {
                let d = a[i * np + i];
                ad[i * np + i] = if d > 1e-300 { d * (1.0 + lambda) } else { lambda.max(1e-300) };
            }
            for i in 0..np {
                neg_g[i] = -g[i];
            }
            let mut ad_try = ad.clone();
            if let Some(d) = gauss_solve(&mut ad_try, &neg_g, np) {
                delta = Some(d);
                break;
            }
            lambda *= 10.0;
        }
        let delta = match delta {
            Some(d) => d,
            None => break,
        };

        for i in 0..np {
            xnew[i] = x[i] + delta[i];
        }
        data.residuals(&xnew, &mut fnew);
        let cost_new = 0.5 * dot(&fnew, &fnew);

        if cost_new < cost {
            let improvement = cost - cost_new;
            x.copy_from_slice(&xnew);
            std::mem::swap(&mut f, &mut fnew);
            cost = cost_new;
            lambda = (lambda * 0.1).max(1e-12);
            // Converged when the cost can no longer improve meaningfully.
            if improvement <= 1e-18 * cost {
                break;
            }
        } else {
            lambda *= 10.0;
            if lambda > 1e16 {
                break;
            }
        }
    }
    cost
}

/// Rough pitch estimate from the marker y positions: with roll = 0 each
/// marker satisfies tan(pitch) ~ tan(elev) + y/scale (y positive down),
/// so average the per-marker solutions. Far better than starting at 0
/// when the camera is tilted well off horizontal.
fn pitch_seed(hills: &[Hill], known: &[usize], scale: f64) -> f64 {
    let mut acc = 0.0;
    let mut n = 0;
    for &i in known {
        let m = &hills[i];
        if scale > 1.0 && m.y.is_finite() {
            acc += (m.a_elev.tan() + m.y / scale).atan();
            n += 1;
        }
    }
    if n > 0 {
        acc / n as f64
    } else {
        0.0
    }
}

/// Run the Levenberg-Marquardt fit for one parameter level and write the
/// result back into `parms`. Mirrors `ProjectionLSQ::lsq`.
fn lsq(proj: &dyn Projection, hills: &[Hill], known: &[usize], parms: &mut ViewParams, level: usize) -> f64 {
    let np = np_for_level(level);
    let data = FitData {
        proj,
        hills,
        known,
        level,
        parms0: *parms,
    };

    let mut x = vec![
        parms.a_center,
        parms.a_pitch,
        parms.a_roll,
        parms.scale,
        parms.k0,
        parms.k1,
        parms.x0,
    ];
    x.truncate(np);

    let cost = lm(&data, &mut x, np);

    parms.a_center = x[0];
    parms.a_pitch = x[1];
    if level > 0 {
        parms.a_roll = x[2];
        parms.scale = x[3];
    }
    if level > 1 {
        parms.k0 = x[4];
        parms.k1 = x[5];
        parms.x0 = x[6];
    }
    cost
}

/// Fit the view parameters to the known hills.
/// Mirrors `ProjectionLSQ::comp_params`. `known` indexes into `hills`.
/// With `fit_distortion == false` the lens distortion parameters
/// (k0/k1/x0) are held at their current values and only the viewpoint
/// parameters are fitted, even for 4+ known hills.
pub fn comp_params(
    proj: &dyn Projection,
    hills: &[Hill],
    known: &[usize],
    parms: &mut ViewParams,
    fit_distortion: bool,
) -> Result<(), String> {
    let n = known.len();
    if n < 1 {
        return Err("Please position at least 1 hill".to_string());
    }

    let m1 = &hills[known[0]];
    let mut new_parms = *parms;
    new_parms.a_center = m1.alph;
    new_parms.a_pitch = 0.0;
    let mut seed_scales: Vec<f64> = Vec::new();

    if n > 1 {
        let m2 = &hills[known[1]];
        // Azimuth difference as SEEN IN THE IMAGE. The shortest-arc wrap
        // alone picks the wrong branch in panoramas wider than 180 deg:
        // markers at -170 deg and +170 deg of the view center are 340 deg
        // apart in the image but only 20 deg apart on the compass. The
        // sign of the pixel difference tells us which branch the user
        // intended; the magnitude is then unique modulo 2pi (panoramas
        // below 360 deg).
        let dx = m2.x - m1.x;
        let two_pi = 2.0 * std::f64::consts::PI;
        let mut d_img = crate::projection::wrap_angle(m2.alph - m1.alph);
        // The wide branch (markers >180 deg apart in the image) only
        // applies to projections that can actually show more than a
        // half-plane; the rectilinear projection is limited to 120 deg
        // of view, so there the shortest arc is always the intended one.
        if proj.get_view_angle() >= std::f64::consts::PI {
            if dx >= 0.0 && d_img <= 0.0 {
                d_img += two_pi;
            } else if dx < 0.0 && d_img >= 0.0 {
                d_img -= two_pi;
            }
        }
        let scale_tmp = dx.abs() / d_img.abs();
        if scale_tmp.is_nan() || scale_tmp < 50.0 {
            return Err(format!("Could not determine initial scale value ({scale_tmp})"));
        }
        new_parms.scale = scale_tmp;
        new_parms.a_roll = 0.0;
        // Start the view center between the two markers (in the image-space
        // branch just chosen) instead of on the first marker: with markers
        // 340 deg apart, m1.alph is 170 deg off and the fit never recovers.
        new_parms.a_center = crate::projection::wrap_angle(m1.alph + d_img / 2.0);
        // Seed scales for the multi-start below: the linear estimate and a
        // tangent-aware variant (rectilinear geometry puts dx = s * 2 *
        // tan(d/2) for a centered pair, which the linear estimate
        // underestimates by up to 60% at wide azimuth spans).
        seed_scales.push(scale_tmp);
        let tan_half = (d_img.abs() / 2.0).tan() * 2.0;
        if tan_half.abs() > 1e-3 {
            let s_tan = (dx.abs() / tan_half).max(50.0);
            if s_tan.is_finite() && (s_tan - scale_tmp).abs() > scale_tmp * 0.02 {
                seed_scales.push(s_tan);
            }
        }
    }

    // Capability probe: the cylindrical projection has no distortion terms
    // (its derivative trait defaults return NaN), so a distortion fit there
    // is impossible - and the calibration reset below would silently zero
    // distortion parameters a rectilinear fit had established.
    let supports_distortion = {
        let probe = crate::projection::ProjArgs::from_params(
            &new_parms,
            hills[known[0]].alph,
            hills[known[0]].a_elev,
        );
        proj.mac_x_dk0(probe).is_finite()
    };

    if n > 3 && fit_distortion && supports_distortion {
        new_parms.k0 = 0.0;
        new_parms.k1 = 0.0;
        new_parms.x0 = 0.0;
        eprintln!("Performing calibration");
    }

    let level = if n == 1 { 0 } else { 1 };
    if n > 1 {
        // Multi-start. The two-marker system is exactly determined and has
        // multiple solution basins: the tangent repeats every 180 deg in
        // the center (camera looking backwards projects identically), and
        // pitch/roll can absorb marker y errors. A single start can land
        // in a spurious minimum depending on where the user last dragged
        // a marker - the "randomly sometimes fit failed" report. Try a
        // small set of seeds and keep the lowest-cost result.
        let mut best: Option<(f64, ViewParams)> = None;
        for &s0 in &seed_scales {
            for &p0 in &[0.0f64, pitch_seed(hills, known, s0)] {
                let mut cand = new_parms;
                cand.scale = s0;
                cand.a_pitch = p0;
                let cost = lsq(proj, hills, known, &mut cand, level);
                if cost.is_finite() && best.as_ref().map_or(true, |(c, _)| cost < *c) {
                    best = Some((cost, cand));
                }
            }
        }
        if let Some((_, p)) = best {
            new_parms = p;
        }
    } else {
        lsq(proj, hills, known, &mut new_parms, level);
    }
    if n >= 4 && fit_distortion && supports_distortion {
        lsq(proj, hills, known, &mut new_parms, 2);
    }

    if new_parms.scale.is_nan() || new_parms.scale < 50.0 {
        return Err("Could not determine reasonable view parameters".to_string());
    }

    // A fit that parks a marked peak outside the projection's own view
    // angle is nonsense: those markers are beyond what the projection can
    // show (rectilinear: +/-60 deg), is_visible() will hide them, and the
    // tangent-compressed solution scatters every other peak. Reject it
    // with a clear message instead of committing garbage parameters.
    // The cylindrical projection's limit is 2pi, so panoramas are
    // unaffected.
    for &i in known {
        let d = crate::projection::wrap_angle(hills[i].alph - new_parms.a_center).abs();
        if d >= proj.get_view_angle() {
            return Err(format!(
                "Fit result outside the projection's view (marker {} at {:.1} deg from center) - is this a panorama? Switch to cylindrical mode",
                hills[i].name,
                d.to_degrees()
            ));
        }
    }

    *parms = new_parms;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::hill::Hill;
    use crate::projection::{Projection, ProjectionType, ViewParams, projection_from_type};

    /// Five observed hills projected with known parameters (incl. distortion).
    fn fixture() -> (Box<dyn Projection>, Vec<Hill>, Vec<usize>, ViewParams) {
        let proj = projection_from_type(ProjectionType::Rectilinear);
        let truth = ViewParams {
            a_center: 0.3,
            scale: 2000.0,
            a_pitch: 0.05,
            a_roll: 0.0,
            k0: -0.0012,
            k1: 2e-5,
            x0: 0.0,
        };
        let mut hills = Vec::new();
        let mut known = Vec::new();
        for i in 0..5 {
            let alph = 0.15 + i as f64 * 0.07;
            let elev = 0.02 + i as f64 * 0.015;
            let (x, y) = proj.get_coordinates(alph, elev, &truth);
            let mut h = Hill::new(&format!("h{i}"), 0.0, 0.0, 1000.0);
            h.alph = alph;
            h.a_elev = elev;
            h.x = x;
            h.y = y;
            hills.push(h);
            known.push(i);
        }
        (proj, hills, known, truth)
    }

    #[test]
    fn locked_distortion_stays_untouched() {
        let (proj, hills, known, truth) = fixture();
        let mut p = truth;
        p.a_center -= 0.01;
        p.scale *= 1.02;
        comp_params(&*proj, &hills, &known, &mut p, false).unwrap();
        // 5 known hills, but the distortion params must not move
        assert_eq!(p.k0, truth.k0);
        assert_eq!(p.k1, truth.k1);
        assert_eq!(p.x0, truth.x0);
        // the viewpoint params did fit
        assert!((p.a_center - truth.a_center).abs() < 1e-6);
    }

    #[test]
    fn unlocked_distortion_fit_recovers() {
        let (proj, hills, known, truth) = fixture();
        let mut p = truth;
        p.k0 = 0.0;
        p.k1 = 0.0;
        p.a_center -= 0.01;
        p.scale *= 1.02;
        comp_params(&*proj, &hills, &known, &mut p, true).unwrap();
        assert!((p.k0 - truth.k0).abs() < 1e-5, "k0={}", p.k0);
        assert!((p.k1 - truth.k1).abs() < 1e-7, "k1={}", p.k1);
    }

    /// Two markers with north between them: one peak west of north
    /// (azimuth 350 deg), one east (10 deg). The raw azimuth difference
    /// is 340 deg; everything must operate on the wrapped 20 deg instead.
    /// Regression test for the panorama-mode fit failure across north
    /// (the cylindrical projection uses m_view - c_view linearly).
    fn north_crossing(ty: ProjectionType) -> (Box<dyn Projection>, Vec<Hill>, Vec<usize>, ViewParams) {
        let proj = projection_from_type(ty);
        let truth = ViewParams {
            a_center: 0.0,
            scale: 2000.0,
            a_pitch: 0.03,
            a_roll: 0.0,
            k0: 0.0,
            k1: 0.0,
            x0: 0.0,
        };
        let mut hills = Vec::new();
        for (i, (az_deg, el)) in [(350.0f64, 0.05f64), (10.0, 0.08)].iter().enumerate() {
            let mut h = Hill::new(&format!("n{i}"), 0.0, 0.0, 2000.0);
            h.alph = az_deg.to_radians();
            h.a_elev = *el;
            let (x, y) = proj.get_coordinates(h.alph, h.a_elev, &truth);
            h.x = x;
            h.y = y;
            hills.push(h);
        }
        (proj, hills, vec![0, 1], truth)
    }

    fn assert_north_crossing_fits(ty: ProjectionType) {
        let (proj, hills, known, truth) = north_crossing(ty);
        let mut p = ViewParams::new();
        comp_params(&*proj, &hills, &known, &mut p, false).unwrap();
        let d_center = crate::projection::wrap_angle(p.a_center - truth.a_center);
        assert!(
            d_center.abs() < 1e-6,
            "{ty:?}: a_center off: got {} rad, want 0",
            p.a_center
        );
        assert!((p.scale - truth.scale).abs() < 1.0, "{ty:?}: scale {}", p.scale);
        assert!((p.a_pitch - truth.a_pitch).abs() < 1e-6, "{ty:?}: pitch {}", p.a_pitch);
    }

    #[test]
    fn fit_across_north_cylindrical() {
        assert_north_crossing_fits(ProjectionType::Cylindrical);
    }

    #[test]
    fn fit_across_north_rectilinear() {
        assert_north_crossing_fits(ProjectionType::Rectilinear);
    }

    #[test]
    fn projection_across_north_uses_shortest_arc() {
        // cylindrical: x is linear in the azimuth difference, so a peak at
        // 350 deg with the center at 0 deg must land at -10 deg * scale,
        // not +350 deg * scale
        let proj = projection_from_type(ProjectionType::Cylindrical);
        let p = ViewParams::new();
        let (x, _) = proj.get_coordinates(350f64.to_radians(), 0.0, &p);
        assert!(
            (x - (-10f64.to_radians() * p.scale)).abs() < 1e-6,
            "x={x}"
        );
    }

    /// Wide panoramas: two markers more than 180 deg apart IN THE IMAGE,
    /// i.e. straddling the seam at +/-180 deg from the view center. The
    /// compass difference is then small (e.g. 20 deg) while the image
    /// difference is large (340 deg); the initial guess must pick the
    /// right branch. Only meaningful for the cylindrical projection -
    /// rectilinear cannot exceed 180 deg of view angle.
    fn wide_panorama(reversed: bool, span_deg: f64) -> (Box<dyn Projection>, Vec<Hill>, Vec<usize>, ViewParams) {
        let proj = projection_from_type(ProjectionType::Cylindrical);
        let truth = ViewParams {
            a_center: 0.0,
            scale: 2000.0,
            a_pitch: 0.03,
            a_roll: 0.0,
            k0: 0.0,
            k1: 0.0,
            x0: 0.0,
        };
        let half = (span_deg / 2.0).to_radians();
        let mut hills = Vec::new();
        for (i, az) in [half, -half].iter().enumerate() {
            let mut h = Hill::new(&format!("w{i}"), 0.0, 0.0, 2500.0);
            h.alph = *az;
            h.a_elev = 0.05 + 0.02 * i as f64;
            let (x, y) = proj.get_coordinates(h.alph, h.a_elev, &truth);
            h.x = x;
            h.y = y;
            hills.push(h);
        }
        let known = if reversed { vec![1, 0] } else { vec![0, 1] };
        (proj, hills, known, truth)
    }

    fn assert_wide_fit(reversed: bool, span_deg: f64) {
        let (proj, hills, known, truth) = wide_panorama(reversed, span_deg);
        let mut p = ViewParams::new();
        comp_params(&*proj, &hills, &known, &mut p, false).unwrap();
        let d = crate::projection::wrap_angle(p.a_center - truth.a_center);
        assert!(
            d.abs() < 1e-6,
            "span {span_deg} rev {reversed}: a_center {} rad",
            p.a_center
        );
        assert!(
            (p.scale - truth.scale).abs() < 1.0,
            "span {span_deg} rev {reversed}: scale {}",
            p.scale
        );
        assert!(
            (p.a_pitch - truth.a_pitch).abs() < 1e-6,
            "span {span_deg} rev {reversed}: pitch {}",
            p.a_pitch
        );
    }

    #[test]
    fn fit_wide_panorama_340_deg_apart() {
        assert_wide_fit(false, 340.0);
    }

    #[test]
    fn fit_wide_panorama_350_deg_apart_reversed_order() {
        assert_wide_fit(true, 350.0);
    }

    #[test]
    fn fit_wide_panorama_just_over_half_circle() {
        assert_wide_fit(false, 200.0);
    }

    /// User scenario: fit distant peaks in panorama mode, switch to
    /// rectilinear (which refits with tangent-based scale), switch back.
    /// Refitting in the cylindrical projection must fully recover the
    /// panorama parameters - the fitted parameters of one projection are
    /// allowed to be garbage for the other, but must never poison a
    /// subsequent refit.
    #[test]
    fn refit_recovers_after_rectilinear_interlude() {
        let (proj, hills, known, truth) = wide_panorama(false, 200.0);
        let mut pano = ViewParams::new();
        comp_params(&*proj, &hills, &known, &mut pano, false).unwrap();

        // reinterpret + refit in rectilinear mode (200 deg exceeds its
        // domain, so the fit is now rejected rather than committing
        // tangent-compressed garbage)
        let rect = projection_from_type(ProjectionType::Rectilinear);
        let mut mixed = pano;
        let _ = comp_params(&*rect, &hills, &known, &mut mixed, false);

        // switch back and refit: panorama parameters must come back
        let mut back = mixed;
        comp_params(&*proj, &hills, &known, &mut back, false).unwrap();
        let d = crate::projection::wrap_angle(back.a_center - truth.a_center);
        assert!(d.abs() < 1e-6, "a_center {}", back.a_center);
        assert!((back.scale - truth.scale).abs() < 1.0, "scale {}", back.scale);
    }

    /// A rectilinear fit must not park markers beyond the projection's
    /// +/-60 deg view: the tangent-compressed solution that matches far
    /// apart markers scatters every other peak (the "jump totally weird"
    /// report). The fit now rejects it with a clear error.
    #[test]
    fn rectilinear_rejects_fit_beyond_view_angle() {
        let proj = projection_from_type(ProjectionType::Rectilinear);
        let truth = ViewParams {
            a_center: 0.0,
            scale: 1000.0,
            ..Default::default()
        };
        // two peaks 150 deg apart in the image (az -75/+75: outside 60 deg)
        let mut hills = Vec::new();
        for (i, az) in [(-75f64).to_radians(), 75f64.to_radians()].iter().enumerate() {
            let mut h = Hill::new(&format!("r{i}"), 0.0, 0.0, 2000.0);
            h.alph = *az;
            h.a_elev = 0.05;
            // pixel positions as if the image were linear (panorama-like)
            h.x = az * 2000.0;
            h.y = -50.0;
            hills.push(h);
        }
        let mut p = ViewParams::new();
        let r = comp_params(&*proj, &hills, &[0, 1], &mut p, false);
        assert!(
            r.is_err(),
            "rectilinear fit of 150 deg-apart markers must be rejected, got center={:.1} deg scale={:.1}",
            p.a_center.to_degrees(),
            p.scale
        );
        let _ = truth;
    }

    /// Same markers in cylindrical mode: legitimate panorama, must fit.
    #[test]
    fn cylindrical_accepts_the_same_wide_markers() {
        let proj = projection_from_type(ProjectionType::Cylindrical);
        let mut hills = Vec::new();
        for (i, az) in [(-75f64).to_radians(), 75f64.to_radians()].iter().enumerate() {
            let mut h = Hill::new(&format!("c{i}"), 0.0, 0.0, 2000.0);
            h.alph = *az;
            h.a_elev = 0.0;
            h.x = az * 2000.0;
            h.y = 0.0;
            hills.push(h);
        }
        let mut p = ViewParams::new();
        comp_params(&*proj, &hills, &[0, 1], &mut p, false).unwrap();
        assert!((p.scale - 2000.0).abs() < 1.0, "scale {}", p.scale);
    }

    /// The cylindrical projection formulas do not contain the distortion
    /// parameters (k0/k1/x0) at all, and its trait derivative defaults are
    /// NaN - so "fit distortion" must be a no-op in panorama mode: the fit
    /// still succeeds and the distortion params stay exactly where they
    /// were. This is what the web app relies on when it greys out the
    /// distortion controls in cylindrical mode.
    #[test]
    fn distortion_fit_is_a_noop_for_cylindrical() {
        let proj = projection_from_type(ProjectionType::Cylindrical);
        let truth = ViewParams {
            a_center: 0.3,
            scale: 2000.0,
            a_pitch: 0.05,
            a_roll: 0.0,
            k0: 0.0,
            k1: 0.0,
            x0: 0.0,
        };
        let mut hills = Vec::new();
        let mut known = Vec::new();
        for i in 0..5 {
            let mut h = Hill::new(&format!("c{i}"), 0.0, 0.0, 2000.0);
            h.alph = 0.15 + i as f64 * 0.07;
            h.a_elev = 0.02 + i as f64 * 0.015;
            let (x, y) = proj.get_coordinates(h.alph, h.a_elev, &truth);
            h.x = x;
            h.y = y;
            hills.push(h);
            known.push(i);
        }
        // start slightly off, with nonzero distortion params that must
        // survive untouched
        let mut p = truth;
        p.a_center -= 0.01;
        p.scale *= 1.02;
        p.k0 = -0.001;
        p.k1 = 1e-5;
        p.x0 = 3.0;
        comp_params(&*proj, &hills, &known, &mut p, true).unwrap();
        assert!(
            (p.a_center - truth.a_center).abs() < 1e-6,
            "a_center {}",
            p.a_center
        );
        assert_eq!(p.k0, -0.001, "k0 moved in cylindrical distortion fit");
        assert_eq!(p.k1, 1e-5, "k1 moved in cylindrical distortion fit");
        assert_eq!(p.x0, 3.0, "x0 moved in cylindrical distortion fit");
    }
}
