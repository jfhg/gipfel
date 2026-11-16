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

// ---- fit diagnostics -------------------------------------------------
// comp_params records what it tried (markers, seeds, costs, verdicts)
// in a per-thread buffer; the web front end drains it into the browser
// console after every refit so users can paste a full session log when
// reporting fit problems. Native callers can ignore it; the buffer is
// capped so undrained runs (tests) stay cheap.

thread_local! {
    static FIT_LOG: std::cell::RefCell<Vec<String>> = const {
        std::cell::RefCell::new(Vec::new())
    };
}

/// Take (and clear) the diagnostics of the comp_params runs since the
/// last call.
pub fn take_fit_log() -> Vec<String> {
    FIT_LOG.with(|l| {
        let mut v = l.borrow_mut();
        std::mem::take(&mut *v)
    })
}

fn flog(s: String) {
    FIT_LOG.with(|l| {
        let mut v = l.borrow_mut();
        if v.len() < 400 {
            v.push(s);
        }
    });
}

/// Log and return an error, so every rejection reason appears in the
/// session log as well as the status line.
fn bail<T>(e: String) -> Result<T, String> {
    flog(format!("REJECT: {e}"));
    Err(e)
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
        return bail("Please position at least 1 hill".to_string());
    }
    flog(format!(
        "comp_params proj_view={:.0}deg n={n} start[center={:.2} pitch={:.2} roll={:.2} scale={:.0} k0={:.5} k1={:.7} x0={:.1}] markers: {}",
        proj.get_view_angle().to_degrees(),
        parms.a_center.to_degrees(),
        parms.a_pitch.to_degrees(),
        parms.a_roll.to_degrees(),
        parms.scale,
        parms.k0,
        parms.k1,
        parms.x0,
        known
            .iter()
            .map(|&i| format!(
                "[{}] {} az={:.2} el={:.2} px=({:.0},{:.0})",
                i,
                hills[i].name,
                hills[i].alph.to_degrees(),
                hills[i].a_elev.to_degrees(),
                hills[i].x,
                hills[i].y
            ))
            .collect::<Vec<_>>()
            .join(" ")
    ));

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
            return bail(format!("Could not determine initial scale value ({scale_tmp})"));
        }
        // Left-right order: a rectilinear image lists peaks in azimuth
        // order (real-world roll stays far below 45 deg). If the second
        // marker sits on the wrong side of the first, no valid fit can
        // exist - say so plainly instead of fitting garbage.
        let daz = crate::projection::wrap_angle(m2.alph - m1.alph);
        if proj.get_view_angle() < std::f64::consts::PI
            && dx.abs() > 5.0
            && daz.abs() > 2f64.to_radians()
            && dx * daz < 0.0
        {
            return bail(format!(
                "Wrong marker order: {} (az {:.1} deg) must be {} of {} (az {:.1} deg) in the image",
                m2.name,
                m2.alph.to_degrees(),
                if daz < 0.0 { "left" } else { "right" },
                m1.name,
                m1.alph.to_degrees()
            ));
        }
        new_parms.scale = scale_tmp;
        new_parms.a_roll = 0.0;
        // Start the view center between the two markers (in the image-space
        // branch just chosen) instead of on the first marker: with markers
        // 340 deg apart, m1.alph is 170 deg off and the fit never recovers.
        new_parms.a_center = crate::projection::wrap_angle(m1.alph + d_img / 2.0);
        // Rectilinear feasibility for same-side marker pairs. With
        // x = s*tan(az - center), two peaks with azimuth separation D that
        // both sit on the SAME side of the image center must satisfy
        // |x_far / x_near| >= min over u of tan(u+D)/tan(u) = tan^2(45deg +
        // D/2). Below that no center/scale exists - the classic case being
        // a panorama (linear pixel spacing) fitted rectinearly. Detect it
        // up front and name the fix instead of letting the optimizer churn
        // and the RMS guard reject generically. The 0.9 tolerance absorbs
        // elevation/pitch/distortion perturbations of the pure tan model.
        if proj.get_view_angle() < std::f64::consts::PI {
            for &i in known {
                for &j in known {
                    if i == j {
                        continue;
                    }
                    let (near, far) = if hills[i].x.abs() <= hills[j].x.abs() {
                        (&hills[i], &hills[j])
                    } else {
                        (&hills[j], &hills[i])
                    };
                    // opposite sides of center: always fittable (center
                    // between the peaks); a near marker at the center
                    // makes the ratio arbitrarily large
                    if near.x * far.x <= 0.0 || near.x.abs() < 5.0 {
                        continue;
                    }
                    let d = crate::projection::wrap_angle(far.alph - near.alph).abs();
                    // tan^2(45deg + D/2); -> infinity as D -> 90deg, which
                    // correctly makes same-side pairs with D >= 90deg
                    // impossible
                    let min_ratio = (std::f64::consts::FRAC_PI_4 + d / 2.0).tan().powi(2);
                    let ratio = far.x.abs() / near.x.abs();
                    if ratio < min_ratio * 0.9 {
                        return bail(format!(
                            "{} is {:.1} times farther from the image center than {} ({:.0} px vs {:.0} px, both {} of center). A pair {:.1} deg apart in azimuth needs a distance ratio of at least {:.0} in rectilinear mode. This looks like a panorama - switch to cylindrical mode, or place the view center between the peaks.",
                            far.name,
                            ratio,
                            near.name,
                            far.x.abs(),
                            near.x.abs(),
                            if far.x >= 0.0 { "right" } else { "left" },
                            d.to_degrees(),
                            min_ratio
                        ));
                    }
                }
            }
        }
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
        flog(format!(
            "d_img={:.2}deg scale0={:.0} center0={:.2}deg scale_seeds=[{}]",
            d_img.to_degrees(),
            scale_tmp,
            new_parms.a_center.to_degrees(),
            seed_scales
                .iter()
                .map(|s| format!("{s:.0}"))
                .collect::<Vec<_>>()
                .join(" ")
        ));
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

    // Sanity domain for fitted solutions: rectilinear is only valid for
    // |delta azimuth| < 90 deg (the tangent flips sign beyond it),
    // cylindrical wraps at 180 deg. Deliberately wider than
    // get_view_angle()'s 60 deg visibility heuristic - wide-angle photos
    // legitimately show peaks 60-85 deg off center.
    let domain = if proj.get_view_angle() >= std::f64::consts::PI {
        std::f64::consts::PI
    } else {
        std::f64::consts::PI / 2.0
    };
    // A candidate fit is only usable if its scale is sane and every known
    // marker stays inside the domain. The exactly-determined two-marker
    // system has several EXACT solutions (cost ties via the tangent's
    // periodicity per marker), and degenerate ones (scale ~ 30 px, center
    // at a marker azimuth +/- 90 deg) match both markers perfectly and
    // would otherwise win the cost race arbitrarily - only to be rejected
    // by the guards below, producing "fit failed" even though a good
    // solution existed. Validity must filter candidates BEFORE the cost
    // comparison, not after.
    let valid = |p: &ViewParams| -> bool {
        if !(p.scale.is_finite() && p.scale >= 50.0) {
            return false;
        }
        known
            .iter()
            .all(|&i| crate::projection::wrap_angle(hills[i].alph - p.a_center).abs() < domain)
    };

    let level = if n == 1 { 0 } else { 1 };
    let mut final_cost;
    if n > 1 {
        // Small seed grid. The exactly-determined two-marker system has
        // multiple solution basins (the tangent repeats every 180 deg in
        // the center), so a handful of principled starts are tried and
        // the best VALID one wins: the derived center, two scale
        // estimates (linear and tangent-aware), with and without the
        // y-derived pitch seed. The validity filter below - not the seed
        // spread - is what keeps degenerate cost-ties from winning.
        let mut best: Option<(f64, ViewParams)> = None;
        let c_seed = new_parms.a_center;
        'seeds: for &s0 in &seed_scales {
            for &p0 in &[0.0f64, pitch_seed(hills, known, s0)] {
                let mut cand = new_parms;
                cand.a_center = c_seed;
                cand.scale = s0;
                cand.a_pitch = p0;
                let cost = lsq(proj, hills, known, &mut cand, level);
                flog(format!(
                    "seed c={:>7.1} s={:>6.0} p={:>6.2} cost={:>12.3} {}",
                    c_seed.to_degrees(),
                    s0,
                    p0.to_degrees(),
                    cost,
                    if valid(&cand) { "valid" } else { "INVALID" }
                ));
                if cost.is_finite() && valid(&cand) && best.as_ref().map_or(true, |(c, _)| cost < *c) {
                    best = Some((cost, cand));
                    // near-exact and valid: stop searching
                    if cost < 1.0 {
                        break 'seeds;
                    }
                }
            }
        }
        match best {
            Some((c, p)) => {
                flog(format!(
                    "chosen cost={c:.3} center={:.3} pitch={:.3} roll={:.3} scale={:.1}",
                    p.a_center.to_degrees(),
                    p.a_pitch.to_degrees(),
                    p.a_roll.to_degrees(),
                    p.scale
                ));
                final_cost = c;
                new_parms = p;
            }
            // No seed produced a valid fit: run the plain seeded fit so
            // the guards below report the most informative reason.
            None => {
                flog("no valid seed result; plain fit for diagnostics".to_string());
                final_cost = lsq(proj, hills, known, &mut new_parms, level);
            }
        }
    } else {
        final_cost = lsq(proj, hills, known, &mut new_parms, level);
        flog(format!(
            "single-marker fit cost={final_cost:.3} center={:.3} pitch={:.3}",
            new_parms.a_center.to_degrees(),
            new_parms.a_pitch.to_degrees()
        ));
    }
    if n >= 4 && fit_distortion && supports_distortion {
        final_cost = lsq(proj, hills, known, &mut new_parms, 2);
        flog(format!(
            "distortion fit cost={final_cost:.3} k0={:.5} k1={:.7} x0={:.1}",
            new_parms.k0,
            new_parms.k1,
            new_parms.x0
        ));
    }

    if new_parms.scale.is_nan() || new_parms.scale < 50.0 {
        return bail(format!(
            "Could not determine reasonable view parameters (scale={:.1})",
            new_parms.scale
        ));
    }

    // The fit must actually MATCH the markers. cost = 0.5 * sum(resid^2)
    // over 2n residuals, so rms = sqrt(cost/n). A huge residual means the
    // marked geometry cannot be represented in this projection mode at
    // all - e.g. a panorama (linear pixel spacing) fitted rectinearly has
    // provably no exact solution for wide spans; the optimizer then
    // commits absurd parameters (pitch -80 deg, scale at the sanity
    // floor) that pass the other guards but never match the markers.
    let rms = (final_cost / n as f64).max(0.0).sqrt();
    if final_cost.is_finite() && rms > 30.0 {
        return bail(format!(
            "Fit cannot match the markers (RMS {rms:.0} px) - this projection cannot represent the marked geometry. Is this a panorama? Switch to cylindrical mode."
        ));
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
    /// true domain (+/-90 deg, where the tangent flips sign). The old
    /// guard used the 60 deg visibility heuristic and rejected perfectly
    /// wide legitimate photos, causing placement-dependent "fit failed"
    /// errors.
    #[test]
    fn rectilinear_rejects_fit_beyond_domain() {
        let proj = projection_from_type(ProjectionType::Rectilinear);
        let mut hills = Vec::new();
        // 200 deg apart: no rectilinear solution can place both within
        // +/-90 deg, so the fit must be rejected
        for (i, az) in [(-100f64).to_radians(), 100f64.to_radians()].iter().enumerate() {
            let mut h = Hill::new(&format!("r{i}"), 0.0, 0.0, 2000.0);
            h.alph = *az;
            h.a_elev = 0.05;
            h.x = az * 2000.0;
            h.y = -50.0;
            hills.push(h);
        }
        let mut p = ViewParams::new();
        let r = comp_params(&*proj, &hills, &[0, 1], &mut p, false);
        assert!(
            r.is_err(),
            "rectilinear fit of 200 deg-apart markers must be rejected, got center={:.1} deg scale={:.1}",
            p.a_center.to_degrees(),
            p.scale
        );
    }

    /// A wide-angle photo (half-FOV ~70 deg) is legitimate rectilinear
    /// territory even though it exceeds the 60 deg visibility heuristic:
    /// the fit must succeed, not fail depending on marker placement.
    #[test]
    fn rectilinear_accepts_wide_angle_photo() {
        let proj = projection_from_type(ProjectionType::Rectilinear);
        let truth = ViewParams {
            a_center: 0.0,
            scale: 1200.0,
            ..Default::default()
        };
        let mut hills = Vec::new();
        for (i, az) in [(-70f64).to_radians(), 70f64.to_radians()].iter().enumerate() {
            let mut h = Hill::new(&format!("w{i}"), 0.0, 0.0, 2000.0);
            h.alph = *az;
            h.a_elev = 0.0;
            let (x, y) = proj.get_coordinates(*az, 0.0, &truth);
            h.x = x;
            h.y = y;
            hills.push(h);
        }
        let mut p = ViewParams::new();
        comp_params(&*proj, &hills, &[0, 1], &mut p, false).unwrap();
        assert!((p.a_center - truth.a_center).abs() < 1e-6, "center {}", p.a_center);
        assert!((p.scale - truth.scale).abs() < 1.0, "scale {}", p.scale);
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

    /// Marking the peaks in the wrong left-right order is impossible to
    /// fit rectinearly (the image lists peaks in azimuth order); the
    /// fitter must say so plainly instead of returning garbage or a
    /// confusing domain error.
    #[test]
    fn wrong_marker_order_is_rejected() {
        let proj = projection_from_type(ProjectionType::Rectilinear);
        let mut hills = Vec::new();
        for (name, az) in [("Zugspitze", 58.4f64), ("Saeuling", 17.5)] {
            let mut h = Hill::new(name, 0.0, 0.0, 2500.0);
            h.alph = az.to_radians();
            h.a_elev = 0.05;
            hills.push(h);
        }
        // Zugspitze marked LEFT of Saeuling - backwards
        hills[0].x = -500.0;
        hills[0].y = -200.0;
        hills[1].x = 500.0;
        hills[1].y = -100.0;
        let mut p = ViewParams::new();
        let e = comp_params(&*proj, &hills, &[0, 1], &mut p, false).unwrap_err();
        assert!(
            e.contains("Wrong marker order") && e.contains("left"),
            "expected order error, got: {e}"
        );
    }

    /// Correct order fits at any sane spacing and on either side of center.
    #[test]
    fn correct_order_fits_at_any_spacing() {
        let proj = projection_from_type(ProjectionType::Rectilinear);
        // (800, 100): both right of center - fittable because the x
        // ratio (8) exceeds the minimum a 40.9 deg azimuth pair can
        // achieve rectinearly (~5); (800, 300) would NOT be fittable.
        for (x1, x2) in [(500.0, -500.0), (2000.0, -2000.0), (800.0, 100.0)] {
            let mut hills = Vec::new();
            for (name, az) in [("Zugspitze", 58.4f64), ("Saeuling", 17.5)] {
                let mut h = Hill::new(name, 0.0, 0.0, 2500.0);
                h.alph = az.to_radians();
                h.a_elev = 0.05;
                hills.push(h);
            }
            hills[0].x = x1;
            // same elevation -> same y (consistent with roll = 0, so an
            // exact solution exists at every spacing)
            hills[0].y = -100.0;
            hills[1].x = x2;
            hills[1].y = -100.0;
            let mut p = ViewParams::new();
            comp_params(&*proj, &hills, &[0, 1], &mut p, false)
                .unwrap_or_else(|e| panic!("spacing {}: {e}", x1 - x2));
        }
    }

    /// Real user session (captured via the fit log): a panorama with
    /// Saeuling az 11.24 at px 150 and Zugspitze az 91.68 at px 3398 -
    /// linear pixel spacing, 80.4 deg apart, both right of center.
    /// Rectilinearly there is NO exact solution (same-side pairs 80.4 deg
    /// apart need a >= 140x pixel ratio; these are 22.7x apart). The
    /// pre-check must catch this and suggest cylindrical mode.
    #[test]
    fn panorama_in_rectilinear_suggests_cylindrical() {
        let proj = projection_from_type(ProjectionType::Rectilinear);
        let mut hills = Vec::new();
        for (name, az, el) in [("SAEULING", 11.24f64, -1.58f64), ("ZUGSPITZE", 91.68, 1.76)] {
            let mut h = Hill::new(name, 0.0, 0.0, 2500.0);
            h.alph = az.to_radians();
            h.a_elev = el.to_radians();
            hills.push(h);
        }
        hills[0].x = 150.0;
        hills[0].y = -493.0;
        hills[1].x = 3398.0;
        hills[1].y = -651.0;
        let mut p = ViewParams::new();
        let e = comp_params(&*proj, &hills, &[0, 1], &mut p, false).unwrap_err();
        assert!(
            e.contains("panorama") && e.contains("cylindrical"),
            "expected panorama suggestion, got: {e}"
        );
        // the same markers fit exactly in cylindrical mode
        let cyl = projection_from_type(ProjectionType::Cylindrical);
        let mut p = ViewParams::new();
        comp_params(&*cyl, &hills, &[0, 1], &mut p, false).unwrap();
    }

    /// Same-side pair just above the feasibility bound fits; just below
    /// it is rejected with the panorama hint (tan^2(45 + 40.9/2) ~ 4.8
    /// minimum ratio for this 40.9 deg pair).
    #[test]
    fn same_side_ratio_bound() {
        let proj = projection_from_type(ProjectionType::Rectilinear);
        let mk = |x_far: f64| {
            let mut hills = Vec::new();
            for (name, az) in [("Zugspitze", 58.4f64), ("Saeuling", 17.5)] {
                let mut h = Hill::new(name, 0.0, 0.0, 2500.0);
                h.alph = az.to_radians();
                h.a_elev = 0.05;
                hills.push(h);
            }
            hills[0].x = x_far;
            hills[0].y = -100.0;
            hills[1].x = 100.0;
            hills[1].y = -100.0;
            hills
        };
        let mut p = ViewParams::new();
        // ratio 10 > 4.8: fittable
        comp_params(&*proj, &mk(1000.0), &[0, 1], &mut p, false).unwrap();
        // ratio 3 < 4.8: impossible, named up front
        let e = comp_params(&*proj, &mk(300.0), &[0, 1], &mut p, false).unwrap_err();
        assert!(e.contains("cylindrical"), "got: {e}");
    }
}
