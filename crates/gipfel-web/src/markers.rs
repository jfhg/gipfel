//! Markers: user observations of hills at image positions, plus the
//! refit entry point. Coordinates are center-relative image pixels,
//! matching the projection output space (same convention as the
//! desktop viewport widget).

use gipfel_core::hill::Hill;
use gipfel_core::panorama::Panorama;

#[derive(Clone, Debug, PartialEq)]
pub struct Marker {
    /// index into `Panorama::hills` (stable: load order is preserved)
    pub hill: usize,
    pub x: f64,
    pub y: f64,
}

/// Refit the projection parameters to the current markers.
/// Same graduated behavior as the original: 1 marker fits the center
/// angle, 2 add the scale, 4+ the full viewpoint (+ distortion when
/// unlocked and enough markers).
pub fn refit(pan: &mut Panorama, markers: &[Marker], fit_distortion: bool) -> Result<(), String> {
    if markers.is_empty() {
        return Ok(());
    }
    let mut obs: Vec<Hill> = Vec::with_capacity(markers.len());
    for m in markers {
        let mut h = pan.hills[m.hill].clone();
        h.x = m.x;
        h.y = m.y;
        obs.push(h);
    }
    let known: Vec<usize> = (0..markers.len()).collect();
    pan.comp_params_with(&obs, &known, fit_distortion)
}

/// Nearest visible hill to image position `(x, y)`, within `max_dist`
/// image pixels. Used to resolve marker clicks.
pub fn nearest_hill(pan: &Panorama, x: f64, y: f64, max_dist: f64) -> Option<usize> {
    let mut best = None;
    let mut best_d = max_dist;
    for &i in &pan.visible {
        let h = &pan.hills[i];
        let d = (h.x - x).hypot(h.y - y);
        if d < best_d {
            best_d = d;
            best = Some(i);
        }
    }
    best
}
