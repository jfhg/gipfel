//! GPS track loading (GPX).
//!
//! Minimal streaming parser built directly on quick-xml: we only need the
//! `trkpt` positions, and real-world files (Garmin etc.) are full of vendor
//! `<extensions>` blocks that stricter parsers choke on. Waypoints and
//! routes are ignored; only track points are read.
//!
//! Security posture for untrusted files:
//! - quick-xml is used with `default-features = false` (no serde, no encoding
//!   detection); it never fetches external DTDs/entities and expands only the
//!   five predefined XML entities, so XXE and entity-expansion ("billion
//!   laughs") attacks are structurally impossible; unknown entity references
//!   make the attribute fail to parse and the point is skipped
//! - input size is capped (MAX_GPX_BYTES) before anything is parsed
//! - the parser only reads lat/lon attributes and <ele> text; all other
//!   elements (including vendor extensions) are ignored without being
//!   interpreted

use crate::hill::{Hill, FLAG_TRACK_POINT};
use quick_xml::events::Event;

/// Reject absurdly large track files (a 64 MiB GPX is already ~500k points).
pub const MAX_GPX_BYTES: u64 = 64 * 1024 * 1024;

/// Load track points from a GPX file (all tracks/segments, in file order).
///
/// Consecutive duplicate positions are dropped; the result is decimated with
/// a uniform stride to at most `max_points` (GPX logs easily reach 10k
/// points, which would bloat the panorama's hill list).
pub fn load_gpx(path: &str, max_points: usize) -> Result<Vec<Hill>, String> {
    let size = std::fs::metadata(path).map_err(|e| format!("{path}: {e}"))?;
    if size.len() > MAX_GPX_BYTES {
        return Err(format!("{path}: file too large ({} bytes)", size.len()));
    }
    let data = std::fs::read_to_string(path).map_err(|e| format!("{path}: {e}"))?;
    let mut reader = quick_xml::Reader::from_str(&data);
    reader.config_mut().trim_text(true);

    let mut pts: Vec<(f64, f64, f64)> = Vec::new(); // (lat, lon, ele) degrees
    let mut depth: usize = 0;
    let mut trkpt_depth: Option<usize> = None;
    let mut in_ele = false;
    let (mut lat, mut lon, mut ele) = (f64::NAN, f64::NAN, 0.0);

    fn push(pts: &mut Vec<(f64, f64, f64)>, lat: f64, lon: f64, ele: f64) {
        if lat.is_nan() || lon.is_nan() {
            return;
        }
        if let Some(&(lata, lona, _)) = pts.last() {
            if lata == lat && lona == lon {
                return; // consecutive duplicate fix
            }
        }
        pts.push((lat, lon, ele));
    }

    loop {
        match reader.read_event().map_err(|e| format!("{path}: {e}"))? {
            Event::Eof => break,
            Event::Empty(e) => {
                if e.local_name().as_ref() == b"trkpt" {
                    let (la, lo) = read_latlon(&e);
                    lat = la;
                    lon = lo;
                    ele = 0.0;
                    push(&mut pts, lat, lon, ele);
                }
            }
            Event::Start(e) => {
                depth += 1;
                match e.local_name().as_ref() {
                    b"trkpt" => {
                        trkpt_depth = Some(depth);
                        ele = 0.0;
                        let (la, lo) = read_latlon(&e);
                        lat = la;
                        lon = lo;
                    }
                    // accept <ele> only as a direct child of <trkpt>, so
                    // elevation-like tags inside vendor extensions are ignored
                    b"ele" => {
                        in_ele = trkpt_depth == Some(depth - 1);
                        if in_ele {
                            ele = 0.0;
                        }
                    }
                    _ => {}
                }
            }
            Event::Text(t) => {
                if in_ele {
                    if let Ok(s) = t.decode() {
                        if let Ok(v) = s.trim().parse() {
                            ele = v;
                        }
                    }
                }
            }
            Event::End(e) => {
                match e.local_name().as_ref() {
                    b"ele" => in_ele = false,
                    b"trkpt" => {
                        push(&mut pts, lat, lon, ele);
                        trkpt_depth = None;
                    }
                    _ => {}
                }
                depth = depth.saturating_sub(1);
            }
            _ => {}
        }
    }

    if pts.is_empty() {
        return Err(format!("{path}: no track points"));
    }

    let stride = pts.len().div_ceil(max_points.max(1));
    Ok(pts
        .iter()
        .step_by(stride)
        .map(|&(lat, lon, ele)| {
            let mut h = Hill::new("track", lat.to_radians(), lon.to_radians(), ele);
            h.flags |= FLAG_TRACK_POINT;
            h
        })
        .collect())
}

fn read_latlon(e: &quick_xml::events::BytesStart) -> (f64, f64) {
    let (mut lat, mut lon) = (f64::NAN, f64::NAN);
    for a in e.attributes().flatten() {
        if let Ok(v) = a.normalized_value(quick_xml::XmlVersion::Implicit1_0) {
            match a.key.local_name().as_ref() {
                b"lat" => lat = v.trim().parse().unwrap_or(f64::NAN),
                b"lon" => lon = v.trim().parse().unwrap_or(f64::NAN),
                _ => {}
            }
        }
    }
    (lat, lon)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tmp(name: &str, content: &str) -> std::path::PathBuf {
        let dir = std::env::temp_dir().join(format!("gipfel-gpx-test-{name}"));
        std::fs::create_dir_all(&dir).unwrap();
        let p = dir.join("t.gpx");
        std::fs::write(&p, content).unwrap();
        p
    }

    #[test]
    fn gpx_track_roundtrip() {
        let gpx = r#"<?xml version="1.0" encoding="UTF-8"?>
<gpx version="1.1" creator="test" xmlns="http://www.topografix.com/GPX/1/1">
  <trk><name>tour</name><trkseg>
    <trkpt lat="47.30" lon="11.10"><ele>1000</ele></trkpt>
    <trkpt lat="47.31" lon="11.11"><ele>1100</ele></trkpt>
    <trkpt lat="47.31" lon="11.11"><ele>1100</ele></trkpt>
    <trkpt lat="47.32" lon="11.12"><ele>1200</ele></trkpt>
  </trkseg></trk>
</gpx>"#;
        let p = tmp("rt", gpx);
        let hills = load_gpx(p.to_str().unwrap(), 100).unwrap();
        assert_eq!(hills.len(), 3); // duplicate dropped
        assert!(hills.iter().all(|h| h.is_track_point()));
        assert!((hills[1].phi - 47.31f64.to_radians()).abs() < 1e-12);
        assert!((hills[2].height - 1200.0).abs() < 1e-9);
    }

    #[test]
    fn gpx_vendor_extensions_ignored() {
        // Garmin-style files are full of vendor extension blocks; they must
        // not confuse the parser (and must not contribute elevation).
        let gpx = r#"<?xml version="1.0" encoding="utf-8"?>
<gpx creator="Garmin Desktop App" version="1.1" xmlns="http://www.topografix.com/GPX/1/1"
     xmlns:gpxtpx="http://www.garmin.com/xmlschemas/TrackPointExtension/v1">
  <metadata><extensions><gpxx:TrackPointExtension xmlns:gpxx="x" version="1.0"/></extensions></metadata>
  <trk><name>T</name><extensions><gpxx2:x xmlns:gpxx2="y"><ele>9999</ele></gpxx2:x></extensions>
    <trkseg>
      <trkpt lat="47.30" lon="11.10">
        <ele>1000</ele>
        <extensions><gpxtpx:TrackPointExtension><gpxtpx:hr>120</gpxtpx:hr></gpxtpx:TrackPointExtension></extensions>
      </trkpt>
      <trkpt lat="47.31" lon="11.11"><ele>1100</ele></trkpt>
    </trkseg>
  </trk>
</gpx>"#;
        let p = tmp("ext", gpx);
        let hills = load_gpx(p.to_str().unwrap(), 100).unwrap();
        assert_eq!(hills.len(), 2);
        assert!((hills[0].height - 1000.0).abs() < 1e-9); // not 9999
    }

    #[test]
    fn gpx_decimation() {
        let mut pts = String::new();
        for i in 0..1000 {
            pts.push_str(&format!(
                "<trkpt lat=\"{}\" lon=\"{}\"><ele>1</ele></trkpt>",
                47.0 + i as f64 * 1e-5,
                11.0 + i as f64 * 1e-5
            ));
        }
        let gpx = format!(
            r#"<?xml version="1.0"?><gpx version="1.1" creator="t" xmlns="http://www.topografix.com/GPX/1/1"><trk><trkseg>{pts}</trkseg></trk></gpx>"#
        );
        let p = tmp("dec", &gpx);
        let hills = load_gpx(p.to_str().unwrap(), 100).unwrap();
        assert!(hills.len() <= 100);
    }

    #[test]
    fn gpx_bom_is_tolerated() {
        // Garmin Desktop App writes a UTF-8 BOM; it must not break parsing.
        let gpx = format!(
            "\u{feff}<?xml version=\"1.0\"?><gpx version=\"1.1\" creator=\"t\" xmlns=\"http://www.topografix.com/GPX/1/1\"><trk><trkseg><trkpt lat=\"47.30\" lon=\"11.10\"/></trkseg></trk></gpx>"
        );
        let p = tmp("bom", &gpx);
        let hills = load_gpx(p.to_str().unwrap(), 100).unwrap();
        assert_eq!(hills.len(), 1);
    }

    #[test]
    fn gpx_entity_injection_is_inert() {
        // XXE / entity-expansion attempts: quick-xml only expands the five
        // predefined entities; unknown references make the attribute fail to
        // parse, so the point is simply skipped (never expanded, never fetched).
        let gpx = r#"<?xml version="1.0"?>
<!DOCTYPE gpx [ <!ENTITY evil "47.30"> <!ENTITY sys SYSTEM "file:///etc/passwd"> ]>
<gpx version="1.1" creator="t" xmlns="http://www.topografix.com/GPX/1/1">
  <trk><trkseg>
    <trkpt lat="&evil;" lon="11.10"><ele>1000</ele></trkpt>
    <trkpt lat="47.31" lon="&sys;"><ele>1100</ele></trkpt>
    <trkpt lat="47.32" lon="11.12"><ele>1200</ele></trkpt>
  </trkseg></trk>
</gpx>"#;
        let p = tmp("ent", gpx);
        let hills = load_gpx(p.to_str().unwrap(), 100).unwrap();
        // only the clean point survives; the entity-bearing ones are skipped
        assert_eq!(hills.len(), 1);
        assert!((hills[0].phi - 47.32f64.to_radians()).abs() < 1e-12);
    }
}
