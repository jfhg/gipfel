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
    let ret = pan.comp_params_with(&obs, &known, fit_distortion);
    // Session diagnostics: dump the core's fit log to the browser
    // console (DevTools) so a failing placement can be analyzed from a
    // copy-paste of the console output.
    let log = gipfel_core::lsq::take_fit_log();
    if !log.is_empty() {
        web_sys::console::log_1(
            &format!(
                "gipfel fit session ({} markers, fit_distortion={fit_distortion})",
                markers.len()
            )
            .into(),
        );
        for line in log {
            web_sys::console::log_1(&format!("  {line}").into());
        }
    }
    if let Err(e) = &ret {
        web_sys::console::log_1(&format!("  => fit failed: {e}").into());
    }
    ret
}

/// Nearest visible hill to image position `(x, y)`, within `max_dist`
/// image pixels. Used to resolve marker clicks. Hills hidden by the
/// occlusion heuristic are skipped unless `show_hidden`.
pub fn nearest_hill(
    pan: &Panorama,
    x: f64,
    y: f64,
    max_dist: f64,
    show_hidden: bool,
) -> Option<usize> {
    let mut best = None;
    let mut best_d = max_dist;
    for &i in &pan.visible {
        let flags = pan.hills[i].flags;
        if flags & gipfel_core::hill::FLAG_DUPLIC != 0
            || flags & gipfel_core::hill::FLAG_TRACK_POINT != 0
        {
            continue;
        }
        if !show_hidden && flags & gipfel_core::hill::FLAG_HIDDEN != 0 {
            continue;
        }
        let h = &pan.hills[i];
        let d = (h.x - x).hypot(h.y - y);
        if d < best_d {
            best_d = d;
            best = Some(i);
        }
    }
    best
}
