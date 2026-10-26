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
/// holds the fitted parameters (length `np`).
fn lm(data: &FitData, x: &mut [f64], np: usize) {
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
}

/// Run the Levenberg-Marquardt fit for one parameter level and write the
/// result back into `parms`. Mirrors `ProjectionLSQ::lsq`.
fn lsq(proj: &dyn Projection, hills: &[Hill], known: &[usize], parms: &mut ViewParams, level: usize) {
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

    lm(&data, &mut x, np);

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

    if n > 1 {
        let m2 = &hills[known[1]];
        let scale_tmp = (m2.x - m1.x).abs() / (m2.alph - m1.alph).abs();
        if scale_tmp.is_nan() || scale_tmp < 50.0 {
            return Err(format!("Could not determine initial scale value ({scale_tmp})"));
        }
        new_parms.scale = scale_tmp;
        new_parms.a_roll = 0.0;
    }

    if n > 3 && fit_distortion {
        new_parms.k0 = 0.0;
        new_parms.k1 = 0.0;
        new_parms.x0 = 0.0;
        eprintln!("Performing calibration");
    }

    let level = if n == 1 { 0 } else { 1 };
    lsq(proj, hills, known, &mut new_parms, level);
    if n >= 4 && fit_distortion {
        lsq(proj, hills, known, &mut new_parms, 2);
    }

    if new_parms.scale.is_nan() || new_parms.scale < 50.0 {
        return Err("Could not determine reasonable view parameters".to_string());
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
}
