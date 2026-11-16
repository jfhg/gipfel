// Regression sweep over VALID two-marker placements: ground-truth
// projections (straddling and same-side pairs, north crossings, pitch
// and roll spread, marker noise, cylindrical wide pairs) fitted from a
// neutral start. Every placement here must fit; this is the net that
// guards the seed/validity logic in comp_params.
use gipfel_core::hill::Hill;
use gipfel_core::projection::{projection_from_type, ProjectionType, ViewParams};

fn mk(pan_az: f64, pan_el: f64, x: f64, y: f64) -> Hill {
    let mut h = Hill::new("h", 0.0, 0.0, 2000.0);
    h.alph = pan_az.to_radians();
    h.a_elev = pan_el.to_radians();
    h.x = x;
    h.y = y;
    h
}

fn fit_two(proj: ProjectionType, hills: &[Hill]) -> Result<ViewParams, String> {
    let p = projection_from_type(proj);
    let mut v = ViewParams::new();
    match gipfel_core::lsq::comp_params(&*p, hills, &[0, 1], &mut v, false) {
        Ok(()) => Ok(v),
        Err(e) => Err(e),
    }
}

#[test]
fn sweep() {
    let mut fails = 0;
    let mut total = 0;
    // Ground-truth generated pairs: project with known params, add noise.
    for &center in &[0.0, 45.0, 90.0, 180.0, 350.0f64] {
        for &scale in &[600.0, 1500.0, 3000.0] {
            for &pitch in &[-15.0, 0.0, 12.0f64] {
                for &span in &[8.0, 25.0, 55.0, 85.0f64] {
                    // straddling center
                    let projr = projection_from_type(ProjectionType::Rectilinear);
                    let truth = ViewParams {
                        a_center: center.to_radians(),
                        scale,
                        a_pitch: pitch.to_radians(),
                        a_roll: 0.05_f64,
                        k0: 0.0,
                        k1: 0.0,
                        x0: 0.0,
                    };
                    let a1 = center - span / 2.0;
                    let a2 = center + span / 2.0;
                    let (x1, y1) = projr.get_coordinates(a1.to_radians(), 0.03, &truth);
                    let (x2, y2) = projr.get_coordinates(a2.to_radians(), 0.06, &truth);
                    for &noise in &[0.0, 5.0f64] {
                        let hills = vec![
                            mk(a1, 0.03, x1 + noise, y1 - noise),
                            mk(a2, 0.06, x2 - noise, y2 + noise),
                        ];
                        total += 1;
                        if fit_two(ProjectionType::Rectilinear, &hills).is_err() {
                            fails += 1;
                            panic!("straddle c={center} s={scale} p={pitch} span={span} n={noise}: {}", fit_two(ProjectionType::Rectilinear, &hills).unwrap_err());
                        }
                    }
                    // same-side pair: both azimuths right of center
                    let a3 = center + 5.0;
                    let a4 = center + 5.0 + span.min(60.0);
                    let (x3, y3) = projr.get_coordinates(a3.to_radians(), 0.03, &truth);
                    let (x4, y4) = projr.get_coordinates(a4.to_radians(), 0.06, &truth);
                    if x3.abs() > 20.0 && x4.abs() > 20.0 && (x4 / x3).is_finite() {
                        let hills = vec![
                            mk(a3, 0.03, x3, y3),
                            mk(a4, 0.06, x4, y4),
                        ];
                        total += 1;
                        if fit_two(ProjectionType::Rectilinear, &hills).is_err() {
                            fails += 1;
                            panic!("same-side c={center} s={scale} p={pitch} span={span}: {}", fit_two(ProjectionType::Rectilinear, &hills).unwrap_err());
                        }
                    }
                }
            }
        }
    }
    // cylindrical wide pairs incl. seam straddle
    for &dc in &[70.0, 150.0, 175.0f64] {
        let cyl = projection_from_type(ProjectionType::Cylindrical);
        let truth = ViewParams {
            a_center: 170.0_f64.to_radians(),
            scale: 200.0,
            a_pitch: -0.1,
            a_roll: 0.0,
            k0: 0.0,
            k1: 0.0,
            x0: 0.0,
        };
        let a1 = 170.0 - dc / 2.0;
        let a2 = 170.0 + dc / 2.0;
        let (x1, y1) = cyl.get_coordinates(a1.to_radians(), 0.03, &truth);
        let (x2, y2) = cyl.get_coordinates(a2.to_radians(), 0.05, &truth);
        let hills = vec![mk(a1, 0.03, x1, y1), mk(a2, 0.05, x2, y2)];
        total += 1;
        if fit_two(ProjectionType::Cylindrical, &hills).is_err() {
            fails += 1;
            panic!("cyl dc={dc}: {}", fit_two(ProjectionType::Cylindrical, &hills).unwrap_err());
        }
    }
    assert_eq!(fails, 0, "{total} placements tested");
}
