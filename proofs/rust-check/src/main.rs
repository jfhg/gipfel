//! Referee for GipfelFind.lean Finding 2 / 2b:
//! grid-search the projection denominator with *Rust's own* f64 trig,
//! then push every exact-zero witness through the real generated
//! projection (ProjectionRectilinear::mac_x / mac_y) and classify the
//! result the way `Panorama::project_point` would (NaN filtered, inf NOT).

use gipfel_core::projection::rectilinear::ProjectionRectilinear;
use gipfel_core::projection::{ProjArgs, Projection};

fn deg(d: f64) -> f64 {
    d * std::f64::consts::PI / 180.0
}

/// The denominator of `_11` in the generated macros:
/// cos(pitch)·cos(elev)·cos(Δα) + sin(pitch)·sin(elev)
fn denom(p: f64, e: f64, d: f64) -> f64 {
    p.cos() * e.cos() * d.cos() + p.sin() * e.sin()
}

fn main() { audit();
    // Full-domain grid: pitch, elev in [-90, 90] step 1°, Δα in [-180, 180] step 2°.
    // (No symmetry reduction: Rust is fast enough for 181·181·181 ≈ 5.9M evals.)
    let mut zeros: Vec<(i32, i32, i32)> = Vec::new();
    let mut min = (f64::MAX, 0.0, 0.0, 0.0);
    for i in -90..=90 {
        for j in -90..=90 {
            for k in (-90)..=90 {
                let p = deg(i as f64);
                let e = deg(j as f64);
                let d = deg(k as f64 * 2.0);
                let v = denom(p, e, d);
                if v == 0.0 {
                    zeros.push((i, j, k * 2));
                } else if v.abs() < min.0 {
                    min = (v.abs(), i as f64, j as f64, k as f64 * 2.0);
                }
            }
        }
    }
    println!("exact zeros in Rust libm: {} (smallest nonzero |denom| = {:.3e} at p={}° e={}° dα={}°)",
        zeros.len(), min.0, min.1, min.2, min.3);

    // Group witnesses into the two predicted families.
    let is_a = |(p, e, d): &(i32, i32, i32)| (d.abs() == 180) && (p + e).abs() == 90;
    let is_b = |(p, e, d): &(i32, i32, i32)| *d == 0 && (p - e).abs() == 90;
    let fam_a = zeros.iter().filter(|w| is_a(w)).count();
    let fam_b = zeros.iter().filter(|w| is_b(w)).count();
    let other = zeros.iter().filter(|w| !is_a(w) && !is_b(w)).count();
    println!("family A (|Δα|=180°, pitch+elev=±90°): {fam_a}");
    println!("family B (Δα=0°,    pitch-elev=±90°): {fam_b}");
    println!("outside both families:                {other}");
    for (p, e, d) in zeros.iter().filter(|w| !is_a(w) && !is_b(w)).take(10) {
        println!("  other witness: pitch {p}° elev {e}° Δα {d}°");
    }

    // Push witnesses through the REAL projection, roll = 0° and roll = 1°.
    let proj = ProjectionRectilinear;
    for roll_deg in [0.0, 1.0] {
        let (mut nan, mut inf, mut fin) = (0, 0, 0);
        let mut ex_inf: Vec<String> = Vec::new();
        let mut ex_nan: Vec<String> = Vec::new();
        for (pi, ei, da) in &zeros {
            let a = ProjArgs {
                c_view: 0.0,
                c_pitch: deg(*pi as f64),
                c_roll: deg(roll_deg),
                scale: 1000.0,
                k0: 0.0,
                k1: 0.0,
                x0: 0.0,
                m_view: deg(*da as f64),
                m_elev: deg(*ei as f64),
            };
            let x = proj.mac_x(a);
            let y = proj.mac_y(a);
            let desc = format!("pitch {pi}° elev {ei}° Δα {da}° roll {roll_deg}° -> x={x} y={y}");
            if x.is_nan() || y.is_nan() {
                nan += 1;
                if ex_nan.len() < 3 { ex_nan.push(desc); }
            } else if !x.is_finite() || !y.is_finite() {
                inf += 1;
                if ex_inf.len() < 3 { ex_inf.push(desc); }
            } else {
                fin += 1;
            }
        }
        println!("\nroll = {roll_deg}°:  NaN (filtered by project_point): {nan}   INF (PASSES the guard!): {inf}   finite: {fin}");
        for s in ex_inf { println!("  INF  {s}"); }
        for s in ex_nan { println!("  NaN  {s}"); }
    }
}

// Appended audit: full witness list + specific checks.
#[allow(dead_code)]
fn audit() {
    let deg = |d: f64| d * std::f64::consts::PI / 180.0;
    let denom = |p: f64, e: f64, d: f64| p.cos() * e.cos() * d.cos() + p.sin() * e.sin();
    let mut zs = Vec::new();
    for i in -90..=90 { for j in -90..=90 { for k in (-90)..=90 {
        if denom(deg(i as f64), deg(j as f64), deg(k as f64 * 2.0)) == 0.0 {
            zs.push((i, j, k * 2));
        }
    }}}
    println!("all {} witnesses:", zs.len());
    for (p, e, d) in &zs { println!("  pitch {p:>3}° elev {e:>3}° Δα {d:>4}°"); }
    // the original Lean witnesses:
    for (p, e, d) in [(6, 84, 180), (16, 74, 180), (32, 58, 180), (36, 54, 180)] {
        let z = denom(deg(p as f64), deg(e as f64), deg(d as f64)) == 0.0;
        println!("Lean witness ({p},{e},{d}) exact zero in Rust? {z}");
    }
}
