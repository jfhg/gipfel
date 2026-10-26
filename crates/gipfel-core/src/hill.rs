//! Hill database: data structures, CSV loading, duplicate detection.
//!
//! Faithful port of gipfel's `Hill.cxx`.

use std::fs::File;
use std::io::{BufRead, BufReader};

pub const FLAG_DUPLIC: u32 = 1 << 0;
pub const FLAG_KNOWN: u32 = 1 << 1;
pub const FLAG_HIDDEN: u32 = 1 << 2;
pub const FLAG_VISIBLE: u32 = 1 << 3;
pub const FLAG_TRACK_POINT: u32 = 1 << 4;
pub const FLAG_EXPORT: u32 = 1 << 5;

pub const PI: f64 = std::f64::consts::PI;
pub const DEG2RAD: f64 = PI / 180.0;

#[derive(Clone, Debug)]
pub struct Hill {
    pub name: String,
    /// latitude, radians
    pub phi: f64,
    /// longitude, radians
    pub lam: f64,
    /// height, meters
    pub height: f64,
    /// azimuth from viewpoint, radians
    pub alph: f64,
    /// peak angle over horizon, radians
    pub a_elev: f64,
    /// distance from viewpoint, fraction of earth radius
    pub dist: f64,
    pub x: f64,
    pub y: f64,
    pub label_x: f64,
    pub label_y: f64,
    pub flags: u32,
}

impl Hill {
    pub fn new(name: &str, phi: f64, lam: f64, height: f64) -> Self {
        Hill {
            name: name.to_string(),
            phi,
            lam,
            height,
            alph: 0.0,
            a_elev: 0.0,
            dist: 0.0,
            x: 0.0,
            y: 0.0,
            label_x: 0.0,
            label_y: 0.0,
            flags: 0,
        }
    }

    pub fn is_track_point(&self) -> bool {
        self.flags & FLAG_TRACK_POINT != 0
    }
}

/// `atof()` semantics: parse the longest numeric prefix, 0.0 on failure.
fn atof(s: &str) -> f64 {
    let s = s.trim();
    if s.is_empty() {
        return 0.0;
    }
    if let Ok(v) = s.parse::<f64>() {
        return v;
    }
    for i in (1..s.len()).rev() {
        if let Ok(v) = s[..i].parse::<f64>() {
            return v;
        }
    }
    0.0
}

/// Load a hill database. Two line formats are accepted:
/// - `SHORT,NAME,DESC,LAT,LON,HEIGHT` (6 fields)
/// - `LAT,LON,HEIGHT` (3 fields, track point)
pub fn load(path: &str) -> std::io::Result<Vec<Hill>> {
    let file = File::open(path)?;
    let mut hills = Vec::new();

    // the database is Latin-1; decode to Unicode code points
    for line in BufReader::new(file).split(b'\n') {
        let raw = line?;
        let line: String = raw.iter().map(|b| *b as char).collect();
        // strsep() semantics: split into at most 10 fields, keeping empties
        let vals: Vec<&str> = line.split(',').take(10).collect();
        let n = vals.len();

        if n == 6 {
            // standard format including name and description
            let phi = atof(vals[3]) * DEG2RAD;
            let lam = atof(vals[4]) * DEG2RAD;
            let height = atof(vals[5]);
            hills.push(Hill::new(vals[1], phi, lam, height));
        } else if n == 3 {
            // track point format
            let phi = atof(vals[0]) * DEG2RAD;
            let lam = atof(vals[1]) * DEG2RAD;
            let height = atof(vals[2]);
            let mut h = Hill::new("", phi, lam, height);
            // NOTE: original does not set TRACK_POINT here
            h.flags |= 0;
            hills.push(h);
        }
    }

    Ok(hills)
}

/// Mark hills that are (near-)exact duplicates of an earlier hill.
/// Faithful port of `Hills::mark_duplicates`.
///
/// Unlike the C original (which sorts the pointer array in place), this
/// must NOT reorder `hills`: in this port, hill indices are stable handles
/// (track lists, known-hill sets, visible lists). Sorting an index
/// permutation instead keeps the semantics identical while preserving
/// insertion order.
pub fn mark_duplicates(hills: &mut [Hill], dist: f64) {
    // descending phi (mirrors the C qsort comparator order)
    let mut order: Vec<usize> = (0..hills.len()).collect();
    order.sort_by(|&a, &b| {
        hills[b]
            .phi
            .partial_cmp(&hills[a].phi)
            .unwrap_or(std::cmp::Ordering::Equal)
    });

    let len = hills.len();
    for pos in 0..len {
        let i = order[pos];
        if hills[i].is_track_point() {
            continue;
        }

        let mut j = pos + 1;
        while j < len && (hills[order[j]].phi - hills[i].phi).abs() <= dist {
            let jj = order[j];
            if hills[jj].flags & FLAG_DUPLIC == 0
                && (hills[jj].lam - hills[i].lam).abs() <= dist
                && (hills[jj].height - hills[i].height).abs() <= 50.0
            {
                hills[jj].flags |= FLAG_DUPLIC;
            }
            j += 1;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn atof_semantics() {
        assert_eq!(atof("12.5"), 12.5);
        assert_eq!(atof(""), 0.0);
        assert_eq!(atof("abc"), 0.0);
        assert_eq!(atof("12abc"), 12.0);
        assert_eq!(atof(" -3.5x"), -3.5);
    }

    #[test]
    fn load_formats() {
        let dir = std::env::temp_dir().join("gipfel-core-test");
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("hills.csv");
        std::fs::write(
            &path,
            "# comment line\n\
             ZSP,Zugspitze,Germany,47.421,10.9855,2962\n\
             FOO,Foo Peak,,47.5,11.0,1000\n\
             47.6,11.1,1234\n\
             bad,line\n",
        )
        .unwrap();

        let hills = load(path.to_str().unwrap()).unwrap();
        assert_eq!(hills.len(), 3);
        assert_eq!(hills[0].name, "Zugspitze");
        assert!((hills[0].phi - 47.421 * DEG2RAD).abs() < 1e-12);
        assert_eq!(hills[0].height, 2962.0);
        assert_eq!(hills[2].name, "");
        let _ = std::fs::remove_file(&path);
    }

    #[test]
    fn duplicates() {
        let mut hills = vec![
            Hill::new("A", 47.0, 11.0, 1000.0),
            Hill::new("A2", 47.000001, 11.000001, 1010.0), // dup of A
            Hill::new("B", 47.5, 11.5, 1000.0),
            Hill::new("C", 47.0, 11.0, 1200.0), // >50m diff -> not dup
        ];
        mark_duplicates(&mut hills, 0.00001);
        assert_eq!(hills.iter().filter(|h| h.flags & FLAG_DUPLIC != 0).count(), 1);
    }

    #[test]
    fn mark_duplicates_preserves_order() {
        // indices are stable handles in the port: mark_duplicates must not
        // reorder the vec (regression: an in-place phi sort broke the app's
        // track index range when add_hills ran it on the full hill list)
        let mut hills = vec![
            Hill::new("south", 46.0, 11.0, 1000.0),
            Hill::new("north", 48.0, 11.0, 1000.0),
            Hill::new("mid", 47.0, 11.0, 1000.0),
            Hill::new("mid-dup", 47.000001, 11.0, 1010.0),
        ];
        mark_duplicates(&mut hills, 0.00001);
        assert_eq!(hills[0].name, "south");
        assert_eq!(hills[1].name, "north");
        assert_eq!(hills[2].name, "mid");
        assert_eq!(hills[3].name, "mid-dup");
        // descending phi visits mid-dup first, so 'mid' is the one marked
        assert_eq!(hills[2].flags & FLAG_DUPLIC, FLAG_DUPLIC);
        assert_eq!(hills[3].flags & FLAG_DUPLIC, 0);
    }
}
