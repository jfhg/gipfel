//! EXPERIMENT: browser spike exposing the gipfel core pipeline to JavaScript.
//!
//! The whole computational core (hill DB, geodesy, projections, LM fit,
//! JPEG comment + EXIF I/O) is pure Rust and WASM-friendly — the GSL
//! removal made this possible. This crate proves the pipeline end-to-end
//! in the browser: bytes in (hill DB + JPEG), fit report out.
//!
//! Build (requires rustup):
//!   rustup target add wasm32-unknown-unknown
//!   cargo install wasm-pack        # once
//!   cd crates/gipfel-wasm && wasm-pack build --target web
//!   python3 -m http.server 8080    # then open /demo/

use wasm_bindgen::prelude::*;

/// Crate version (from Cargo.toml, via the workspace).
#[wasm_bindgen]
pub fn version() -> String {
    env!("CARGO_PKG_VERSION").to_string()
}

/// End-to-end pipeline smoke test, all in memory:
///
/// 1. parse the hill database (`db` = gipfel.dat bytes, Latin-1 CSV)
/// 2. read gipfel metadata from the JPEG bytes (comment, else EXIF)
/// 3. build the panorama, take up to 5 visible hills as synthetic
///    "user-marked" observations, perturb the view parameters, refit
/// 4. return a JSON report (recovered parameters + max reprojection error)
///
/// This mirrors the native golden fit test; a correct WASM build reports
/// a reprojection error well below one pixel.
#[wasm_bindgen]
pub fn fit_demo(db: &[u8], jpg: &[u8]) -> String {
    match fit_inner(db, jpg) {
        Ok(json) => json,
        Err(e) => format!(r#"{{"ok":false,"error":"{}"}}"#, e.replace('\\', "\\\\").replace('"', "\\\"")),
    }
}

fn fit_inner(db: &[u8], jpg: &[u8]) -> Result<String, String> {
    use gipfel_core::metadata::{apply_image_metadata, ImageMetaData};
    use gipfel_core::panorama::Panorama;
    use gipfel_core::projection::projection_from_type;

    let mut pan = Panorama::new();
    pan.load_data_from_bytes(db).map_err(|e| format!("hill db: {e}"))?;

    let mut md = ImageMetaData::new();
    md.load_image_from_bytes(jpg);

    let (w, h) = image::ImageReader::new(std::io::Cursor::new(jpg))
        .with_guessed_format()
        .map_err(|e| format!("image: {e}"))?
        .into_dimensions()
        .map_err(|e| format!("image: {e}"))?;
    if w == 0 || h == 0 {
        return Err("image has zero dimensions".into());
    }

    let have = apply_image_metadata(&mut pan, &md, w, h);
    if !have {
        // demo fallback: a fixed Alpine viewpoint so the pipeline always runs
        pan.set_view_lat(47.36);
        pan.set_view_long(11.15);
        pan.set_view_height(2100.0);
        pan.set_center_angle(20.0);
        pan.set_pitch_angle(0.0);
        pan.set_roll_angle(1.0);
        pan.set_scale(3000.0);
    }

    let n = pan.visible.len().min(5);
    if n < 2 {
        return Err("fewer than 2 hills visible from this viewpoint".into());
    }
    // snapshot the "observations" (what a user would have marked)
    let obs: Vec<_> = pan.visible[..n].iter().map(|&i| pan.hills[i].clone()).collect();
    let known: Vec<usize> = (0..n).collect();

    // perturb, then refit (distortion stays locked for the spike)
    let center0 = pan.get_center_angle();
    let scale0 = pan.get_scale();
    pan.set_center_angle(center0 + 0.5);
    pan.set_scale(scale0 * 0.95);
    pan.comp_params_with(&obs, &known, false)?;

    // reprojection error of the recovered parameters against the snapshot
    let proj = projection_from_type(pan.projection_type);
    let mut max_err = 0.0f64;
    for ob in &obs {
        let (x, y) = proj.get_coordinates(ob.alph, ob.a_elev, &pan.parms);
        max_err = max_err.max((x - ob.x).hypot(y - ob.y));
    }

    Ok(format!(
        concat!(
            "{{\"ok\":true,\"hills_in_db\":{},\"image\":\"{}x{}\",\"had_gipfel_info\":{},",
            "\"observations\":{},\"viewpoint\":{{\"lat\":{:.5},\"lon\":{:.5},\"height_m\":{:.0}}},",
            "\"perturbed\":{{\"center_deg\":{:.4},\"scale\":{:.1}}},",
            "\"refitted\":{{\"center_deg\":{:.6},\"pitch_deg\":{:.6},\"roll_deg\":{:.6},\"scale\":{:.2}}},",
            "\"max_reproj_err_px\":{:.4}}}"
        ),
        pan.hills.len(),
        w,
        h,
        have,
        n,
        pan.get_view_lat(),
        pan.get_view_long(),
        pan.get_view_height(),
        center0 + 0.5,
        scale0 * 0.95,
        pan.get_center_angle(),
        pan.get_pitch_angle(),
        pan.get_roll_angle(),
        pan.get_scale(),
        max_err,
    ))
}

#[cfg(test)]
mod tests {
    use crate::fit_demo;

    /// The spike must also work natively (the wasm-bindgen attribute is a
    /// no-op off-wasm), so CI here covers the whole byte-oriented pipeline.
    /// Skips when the repo data files are not present (e.g. stripped checkouts).
    #[test]
    fn fit_demo_native_pipeline() {
        let (Ok(db), Ok(jpg)) = (
            std::fs::read("../../gipfel.dat"),
            std::fs::read("../../tests/golden/test.jpg"),
        ) else {
            eprintln!("gipfel.dat / test.jpg not found, skipping");
            return;
        };
        let out = fit_demo(&db, &jpg);
        assert!(out.contains(r#""ok":true"#), "fit_demo failed: {out}");
        // recovered parameters must reproduce the observations sub-pixel
        let err = out
            .split("max_reproj_err_px\":")
            .nth(1)
            .and_then(|s| s.trim_end_matches('}').parse::<f64>().ok())
            .expect("error value present");
        assert!(err < 0.01, "reprojection error too large: {out}");
    }
}
