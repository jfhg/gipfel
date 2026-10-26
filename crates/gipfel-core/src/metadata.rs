//! Image metadata: the `gipfel:` JPEG comment (read/write, byte-compatible
//! with the C version) plus EXIF fallback (GPS position, focal length).
//!
//! Faithful port of gipfel's `ImageMetaData.cxx`.

use std::fs::File;
use std::io::{BufReader, Write};

use exif::{In, Tag, Value};

pub const NAN: f64 = f64::NAN;

#[derive(Clone, Debug)]
pub struct ImageMetaData {
    pub manufacturer: Option<String>,
    pub model: Option<String>,
    pub longitude: f64,
    pub latitude: f64,
    pub height: f64,
    pub direction: f64,
    /// camera pitch, degrees (written as the "nick" key in the gipfel:
    /// comment for backwards compatibility with the C original)
    pub pitch: f64,
    /// camera roll, degrees (written as the "tilt" key for compatibility)
    pub roll: f64,
    pub k0: f64,
    pub k1: f64,
    pub x0: f64,
    pub focal_length: f64,
    pub focal_length_35mm: f64,
    pub projection_type: i32,
}

impl Default for ImageMetaData {
    fn default() -> Self {
        ImageMetaData {
            manufacturer: None,
            model: None,
            longitude: NAN,
            latitude: NAN,
            height: NAN,
            direction: NAN,
            pitch: NAN,
            roll: NAN,
            k0: NAN,
            k1: NAN,
            x0: NAN,
            focal_length: NAN,
            focal_length_35mm: NAN,
            projection_type: 0,
        }
    }
}

impl ImageMetaData {
    pub fn new() -> Self {
        Self::default()
    }

    /// Read JPEG comment first, then fill remaining gaps from EXIF.
    pub fn load_image(&mut self, path: &str) {
        *self = ImageMetaData::new();
        if let Some(com) = read_jpeg_comment(path) {
            self.parse_gipfel_comment(&com);
        }
        self.load_image_exif(path);
    }

    /// Write the gipfel comment into a copy of `in_img`, saved as `out_img`.
    /// Writes atomically via a temp file, like the C original.
    pub fn save_image(&self, in_img: &str, out_img: &str) -> std::io::Result<()> {
        let comment = self.format_comment();
        let tmp = format!("{}.tmp", out_img);
        write_jpeg_comment(in_img, &tmp, comment.as_bytes())?;
        std::fs::rename(&tmp, out_img)?;
        Ok(())
    }

    pub fn format_comment(&self) -> String {
        format!(
            "gipfel: longitude {}, latitude {}, height {}, direction {}, nick {}, tilt {}, focal_length_35mm {}, projection type {}, k0 {}, k1 {}, x0 {}",
            fmt_f64(self.longitude),
            fmt_f64(self.latitude),
            fmt_f64(self.height),
            fmt_f64(self.direction),
            fmt_f64(self.pitch),
            fmt_f64(self.roll),
            fmt_f64(self.focal_length_35mm),
            self.projection_type,
            fmt_f64(self.k0),
            fmt_f64(self.k1),
            fmt_f64(self.x0),
        )
    }

    /// sscanf(gipfel_format_scan, ...) equivalent; returns true when at
    /// least 8 fields were matched (the C requirement).
    pub fn parse_gipfel_comment(&mut self, com: &[u8]) -> bool {
        let s = String::from_utf8_lossy(com);
        let mut sc = Scanner { s: s.as_ref() };

        if !sc.lit("gipfel: longitude ") {
            return false;
        }
        let Some(lo) = sc.f64() else { return false };
        if !sc.lit(", latitude ") { return false; }
        let la = sc.f64();
        if la.is_none() { return false; }
        if !sc.lit(", height ") { return false; }
        let he = sc.f64();
        if he.is_none() { return false; }
        if !sc.lit(", direction ") { return false; }
        let dir = sc.f64();
        if dir.is_none() { return false; }
        if !sc.lit(", nick ") { return false; }
        let pit = sc.f64();
        if pit.is_none() { return false; }
        if !sc.lit(", tilt ") { return false; }
        let rol = sc.f64();
        if rol.is_none() { return false; }
        if !sc.lit(", focal_length_35mm ") { return false; }
        let fr = sc.f64();
        if fr.is_none() { return false; }
        if !sc.lit(", projection type ") { return false; }
        let pt = sc.i32();

        // 8 matches required: through projection type
        let Some(pt) = pt else { return false };

        self.longitude = lo;
        self.latitude = la.unwrap();
        self.height = he.unwrap();
        self.direction = dir.unwrap();
        self.pitch = pit.unwrap();
        self.roll = rol.unwrap();
        self.focal_length_35mm = fr.unwrap();
        self.projection_type = pt;

        // n >= 10: k0 and k1 were matched; x0 defaults to 0.0 when only 10
        if let Some(k0) = sc.f64_after(", k0 ") {
            if let Some(k1) = sc.f64_after(", k1 ") {
                self.k0 = k0;
                self.k1 = k1;
                self.x0 = sc.f64_after(", x0 ").unwrap_or(0.0);
            }
        }

        true
    }

    fn load_image_exif(&mut self, path: &str) {
        let Ok(file) = File::open(path) else { return };
        let mut reader = BufReader::new(file);
        let Ok(exif) = exif::Reader::new().read_from_container(&mut reader) else {
            return;
        };

        if self.manufacturer.is_none() {
            if let Some(f) = exif.get_field(Tag::Make, In::PRIMARY) {
                let s = field_string(f);
                if !s.is_empty() {
                    self.manufacturer = Some(s);
                }
            }
        }
        if self.model.is_none() {
            if let Some(f) = exif.get_field(Tag::Model, In::PRIMARY) {
                let s = field_string(f);
                if !s.is_empty() {
                    self.model = Some(s);
                }
            }
        }

        if self.focal_length.is_nan() {
            exif_set_value(self, &exif, Tag::FocalLength, In::PRIMARY, |md, v| md.focal_length = v);
        }
        if self.focal_length_35mm.is_nan() {
            exif_set_value(
                self,
                &exif,
                Tag::FocalLengthIn35mmFilm,
                In::PRIMARY,
                |md, v| md.focal_length_35mm = v,
            );
        }
        if self.longitude.is_nan() {
            exif_set_coordinate(self, &exif, Tag::GPSLongitude, In::PRIMARY, |md, v| md.longitude = v);
            if !self.longitude.is_nan() && exif_ref_is(&exif, Tag::GPSLongitudeRef, "W") {
                self.longitude = -self.longitude;
            }
        }
        if self.latitude.is_nan() {
            exif_set_coordinate(self, &exif, Tag::GPSLatitude, In::PRIMARY, |md, v| md.latitude = v);
            if !self.latitude.is_nan() && exif_ref_is(&exif, Tag::GPSLatitudeRef, "S") {
                self.latitude = -self.latitude;
            }
        }
        if self.height.is_nan() {
            exif_set_value(self, &exif, Tag::GPSAltitude, In::PRIMARY, |md, v| md.height = v);
            // GPSAltitudeRef: 0 = above sea level, 1 = below
            if !self.height.is_nan() {
                if let Some(f) = exif.get_field(Tag::GPSAltitudeRef, In::PRIMARY) {
                    if let Value::Byte(b) = &f.value {
                        if b.first() == Some(&1) {
                            self.height = -self.height;
                        }
                    }
                }
            }
        }
    }
}

/// True if an EXIF ASCII reference tag (GPSLatitudeRef etc.) equals `val`.
fn exif_ref_is(exif: &exif::Exif, tag: Tag, val: &str) -> bool {
    exif.get_field(tag, In::PRIMARY)
        .and_then(|f| match &f.value {
            Value::Ascii(a) => a.first().map(|v| String::from_utf8_lossy(v).trim() == val),
            _ => None,
        })
        .unwrap_or(false)
}

fn fmt_f64(v: f64) -> String {
    if v.is_nan() {
        // C printf("%f", nan) prints "nan"; keep it parseable by both
        "nan".to_string()
    } else {
        format!("{:.6}", v)
    }
}

fn field_string(f: &exif::Field) -> String {
    let join = |v: &[String]| v.join(" ");
    match &f.value {
        Value::Ascii(a) => a
            .first()
            .map(|v| String::from_utf8_lossy(v).trim().to_string())
            .unwrap_or_default(),
        Value::Rational(r) => join(r.iter().map(|v| v.to_string()).collect::<Vec<_>>().as_slice()),
        Value::Short(v) => join(v.iter().map(|v| v.to_string()).collect::<Vec<_>>().as_slice()),
        Value::Long(v) => join(v.iter().map(|v| v.to_string()).collect::<Vec<_>>().as_slice()),
        Value::SShort(v) => join(v.iter().map(|v| v.to_string()).collect::<Vec<_>>().as_slice()),
        Value::SLong(v) => join(v.iter().map(|v| v.to_string()).collect::<Vec<_>>().as_slice()),
        _ => String::new(),
    }
}

/// exifSetValue: assign when the tag exists and its value is >= 0.
fn exif_set_value<F>(md: &mut ImageMetaData, exif: &exif::Exif, tag: Tag, inp: In, set: F)
where
    F: FnOnce(&mut ImageMetaData, f64),
{
    if let Some(f) = exif.get_field(tag, inp) {
        if let Some(v) = field_tofloat(f, 0) {
            if v >= 0.0 {
                set(md, v);
            }
        }
    }
}

/// exifSetCoordinate: degrees + minutes/60 + seconds/3600, each component
/// only used when >= 0 (faithful to the C original, incl. ignoring N/S refs).
fn exif_set_coordinate<F>(md: &mut ImageMetaData, exif: &exif::Exif, tag: Tag, inp: In, set: F)
where
    F: FnOnce(&mut ImageMetaData, f64),
{
    let Some(f) = exif.get_field(tag, inp) else {
        return;
    };

    let mut dest: Option<f64> = None;
    for i in 0..3 {
        if let Some(v) = field_tofloat(f, i) {
            if v >= 0.0 {
                dest = Some(match i {
                    0 => v,
                    1 => dest.unwrap_or(0.0) + v / 60.0,
                    _ => dest.unwrap_or(0.0) + v / 3600.0,
                });
            }
        }
    }
    if let Some(v) = dest {
        set(md, v);
    }
}

fn field_tofloat(f: &exif::Field, idx: usize) -> Option<f64> {
    match &f.value {
        Value::Rational(r) => r.get(idx).map(|v| v.num as f64 / v.denom as f64),
        Value::Short(v) => v.get(idx).map(|v| *v as f64),
        Value::Long(v) => v.get(idx).map(|v| *v as f64),
        Value::SShort(v) => v.get(idx).map(|v| *v as f64),
        Value::SLong(v) => v.get(idx).map(|v| *v as f64),
        _ => None,
    }
}

// ---------------------------------------------------------------------------
// strtod-like scanner for the sscanf format
// ---------------------------------------------------------------------------

struct Scanner<'a> {
    s: &'a str,
}

impl Scanner<'_> {
    fn skip_ws(&mut self) {
        self.s = self.s.trim_start();
    }

    fn lit(&mut self, lit: &str) -> bool {
        if self.s.starts_with(lit) {
            self.s = &self.s[lit.len()..];
            true
        } else {
            false
        }
    }

    /// %lf: skip whitespace, parse the longest numeric prefix.
    fn f64(&mut self) -> Option<f64> {
        self.skip_ws();
        for i in (1..=self.s.len()).rev() {
            if let Ok(v) = self.s[..i].parse::<f64>() {
                self.s = &self.s[i..];
                return Some(v);
            }
        }
        None
    }

    fn f64_after(&mut self, lit: &str) -> Option<f64> {
        if self.lit(lit) {
            self.f64()
        } else {
            None
        }
    }

    /// %d
    fn i32(&mut self) -> Option<i32> {
        self.skip_ws();
        for i in (1..=self.s.len()).rev() {
            if let Ok(v) = self.s[..i].parse::<i32>() {
                self.s = &self.s[i..];
                return Some(v);
            }
        }
        None
    }
}

// ---------------------------------------------------------------------------
// JPEG COM segment reading / writing
// ---------------------------------------------------------------------------

fn marker_len_ok(marker: u8) -> bool {
    // standalone markers without length field
    marker == 0x01 || (0xD0..=0xD9).contains(&marker)
}

/// Return the payload of the first COM segment before SOS, if any.
pub fn read_jpeg_comment(path: &str) -> Option<Vec<u8>> {
    let bytes = std::fs::read(path).ok()?;
    if bytes.len() < 2 || bytes[0] != 0xFF || bytes[1] != 0xD8 {
        return None;
    }

    let mut i = 2;
    while i + 1 < bytes.len() {
        if bytes[i] != 0xFF {
            return None;
        }
        let marker = bytes[i + 1];
        if marker == 0xFF {
            i += 1;
            continue;
        }
        if marker == 0xDA || marker == 0xD9 {
            return None;
        }
        if marker_len_ok(marker) {
            i += 2;
            continue;
        }
        if i + 4 > bytes.len() {
            return None;
        }
        let len = (u16::from_be_bytes([bytes[i + 2], bytes[i + 3]]) as usize).checked_add(2)?;
        if i + len > bytes.len() {
            return None;
        }
        if marker == 0xFE {
            return Some(bytes[i + 4..i + 2 + len - 2].to_vec());
        }
        i += len;
    }
    None
}

/// Copy `in_img` to `out_img`, dropping all COM segments and inserting a
/// fresh COM segment right after SOI.
pub fn write_jpeg_comment(in_img: &str, out_img: &str, comment: &[u8]) -> std::io::Result<()> {
    let bytes = std::fs::read(in_img)?;
    if bytes.len() < 2 || bytes[0] != 0xFF || bytes[1] != 0xD8 {
        return Err(std::io::Error::new(std::io::ErrorKind::InvalidInput, "not a JPEG file"));
    }
    if comment.len() + 2 > 0xFFFF {
        return Err(std::io::Error::new(std::io::ErrorKind::InvalidInput, "comment too long"));
    }

    let mut out: Vec<u8> = Vec::with_capacity(bytes.len() + comment.len() + 4);
    out.extend_from_slice(&bytes[..2]); // SOI

    // Copy the SOI-adjacent header region (APPn/COM/DPI/etc.), dropping COM
    // segments, and insert our fresh COM after the last APPn segment:
    // that is where exiv2 (and thus the C gipfel) expects it.
    let mut i = 2;
    let mut insert_at = out.len();
    let mut copy_rest_from = bytes.len();
    while i + 1 < bytes.len() {
        if bytes[i] != 0xFF {
            break;
        }
        let marker = bytes[i + 1];
        if marker == 0xFF {
            i += 1;
            continue;
        }
        if marker == 0xDA || marker == 0xD9 {
            copy_rest_from = i;
            break;
        }
        if marker_len_ok(marker) {
            out.push(0xFF);
            out.push(marker);
            i += 2;
            continue;
        }
        if i + 4 > bytes.len() {
            break;
        }
        let len = match u16::from_be_bytes([bytes[i + 2], bytes[i + 3]]) {
            l => (l as usize).checked_add(2).unwrap_or(usize::MAX),
        };
        if len < 2 || i + len > bytes.len() {
            break;
        }
        if marker != 0xFE {
            out.extend_from_slice(&bytes[i..i + len]);
            if (0xE0..=0xEF).contains(&marker) {
                insert_at = out.len();
            }
        }
        i += len;
    }
    copy_rest_from = copy_rest_from.min(i.max(2));

    let mut com: Vec<u8> = Vec::with_capacity(comment.len() + 4);
    com.push(0xFF);
    com.push(0xFE);
    com.extend_from_slice(&((comment.len() + 2) as u16).to_be_bytes());
    com.extend_from_slice(comment);
    out.splice(insert_at..insert_at, com);

    out.extend_from_slice(&bytes[copy_rest_from..]);

    let mut f = File::create(out_img)?;
    f.write_all(&out)?;
    f.flush()
}

/// Read raw image dimensions (width, height) of a JPEG.
pub fn image_dimensions(path: &str) -> Option<(u32, u32)> {
    image::image_dimensions(path).ok()
}

/// Apply image metadata to a panorama like `GipfelWidget::load_image` does.
/// Returns false when the gipfel info is incomplete (C: `have_gipfel_info`).
pub fn apply_image_metadata(
    pan: &mut crate::panorama::Panorama,
    md: &ImageMetaData,
    img_w: u32,
    img_h: u32,
) -> bool {
    use crate::projection::ProjectionType;

    let mut have_info = true;

    pan.set_view_long(md.longitude);
    pan.set_view_lat(md.latitude);
    pan.set_view_height(md.height);

    pan.set_projection(match md.projection_type {
        1 => ProjectionType::Cylindrical,
        _ => ProjectionType::Rectilinear,
    });

    if md.direction.is_nan() {
        pan.set_center_angle(0.0);
        have_info = false;
    } else {
        pan.set_center_angle(md.direction);
    }

    if md.pitch.is_nan() {
        pan.set_pitch_angle(0.0);
        have_info = false;
    } else {
        pan.set_pitch_angle(md.pitch);
    }

    if md.roll.is_nan() {
        pan.set_roll_angle(0.0);
        have_info = false;
    } else {
        pan.set_roll_angle(md.roll);
    }

    // set_focal_length_35mm: scale = fl * max(w, h) / 35
    let w = img_w.max(img_h);
    if md.focal_length_35mm.is_nan() || md.focal_length_35mm == 0.0 {
        pan.set_scale(35.0 * w as f64 / 35.0);
        have_info = false;
    } else {
        pan.set_scale(md.focal_length_35mm * w as f64 / 35.0);
    }

    // distortion parameters, C ordering:
    // 1. gipfel data in JPEG comment
    // 2. matching distortion profile (camera + focal length)
    // 3. 0.0, 0.0, 0.0
    let (mut k0, mut k1, mut x0) = (md.k0, md.k1, md.x0);
    if k0.is_nan() {
        if let Some(p) = md
            .manufacturer
            .as_deref()
            .zip(md.model.as_deref())
            .and_then(|(mfr, model)| crate::profile::profile_name(mfr, model, md.focal_length))
            .and_then(|name| crate::profile::load(&name).ok())
        {
            k0 = p.k0;
            k1 = p.k1;
            x0 = p.x0;
        }
    }
    if k0.is_nan() {
        k0 = 0.0;
    }
    if k1.is_nan() {
        k1 = 0.0;
    }
    if x0.is_nan() {
        x0 = 0.0;
    }
    pan.set_distortion_params(k0, k1, x0);

    have_info
}

/// The profile name auto-matched from this image's camera and focal length
/// (C `get_distortion_profile_name`). Suggested whenever the camera is
/// known — [`apply_image_metadata`] only *applies* it when the JPEG
/// comment carries no distortion params.
pub fn distortion_profile_name(md: &ImageMetaData) -> Option<String> {
    md.manufacturer
        .as_deref()
        .zip(md.model.as_deref())
        .and_then(|(mfr, model)| crate::profile::profile_name(mfr, model, md.focal_length))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn jpeg_comment_write_read() {
        let dir = std::env::temp_dir().join("gipfel-core-mdtest");
        std::fs::create_dir_all(&dir).unwrap();
        let img = dir.join("t.jpg");
        let out = dir.join("o.jpg");

        image::DynamicImage::new_rgb8(64, 48)
            .grayscale()
            .save(&img)
            .unwrap();

        let comment = b"gipfel: longitude 1.000000, latitude 2.000000, height 3.000000, direction 4.000000, nick 5.000000, tilt 6.000000, focal_length_35mm 7.000000, projection type 0, k0 0.000000, k1 0.000000, x0 0.000000";
        write_jpeg_comment(img.to_str().unwrap(), out.to_str().unwrap(), comment).unwrap();

        // no COM before, one after
        assert!(read_jpeg_comment(img.to_str().unwrap()).is_none());
        assert_eq!(read_jpeg_comment(out.to_str().unwrap()).unwrap(), comment);

        // rewriting replaces the existing comment
        let out2 = dir.join("o2.jpg");
        write_jpeg_comment(out.to_str().unwrap(), out2.to_str().unwrap(), b"short").unwrap();
        assert_eq!(read_jpeg_comment(out2.to_str().unwrap()).unwrap(), b"short");

        // image still decodes
        assert!(image::image_dimensions(out.to_str().unwrap()).is_ok());

        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn distortion_precedence_and_legacy_import() {
        use crate::panorama::Panorama;
        use crate::profile;

        let dir = std::env::temp_dir().join(format!("gipfel-prec-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        let data = dir.join("data");
        let home = dir.join("home");
        std::fs::create_dir_all(&data).unwrap();

        let old = (std::env::var("HOME").ok(), std::env::var("XDG_DATA_HOME").ok());
        std::env::set_var("HOME", &home);
        std::env::set_var("XDG_DATA_HOME", &data);

        // legacy FLTK prefs file (C version's location and format)
        let legacy = home
            .join(".fltk")
            .join("Johannes.HofmannATgmx.de")
            .join("gipfel")
            .join("DistortionProfiles.prefs");
        std::fs::create_dir_all(legacy.parent().unwrap()).unwrap();
        std::fs::write(
            &legacy,
            "; FLTK preferences file format 1.0\n; vendor: Johannes.HofmannATgmx.de\n\
             ; application: gipfel/DistortionProfiles\n\n[.]\n\n\n\
             [./Canon_EOS_5D_50.00_mm]\n\nk0:-0.01\nk1:0.001\nx0:3\n",
        )
        .unwrap();

        // first access imports into the XDG data dir
        assert!(!profile::prefs_path().exists());
        let all = profile::list();
        assert!(all.iter().any(|(n, p)| n == "Canon_EOS_5D_50.00_mm" && p.k0 == -0.01));
        assert!(profile::prefs_path().starts_with(&data));
        assert!(profile::prefs_path().exists());

        let mut md = ImageMetaData::new();
        md.manufacturer = Some("Canon".into());
        md.model = Some("EOS_5D".into());
        md.focal_length = 50.0;
        let mut pan = Panorama::new();

        // 1. comment wins when present
        md.k0 = -0.9;
        md.k1 = 0.0;
        md.x0 = 0.0;
        apply_image_metadata(&mut pan, &md, 1000, 800);
        assert!((pan.parms.k0 + 0.9).abs() < 1e-12, "comment should win");

        // 2. profile used when comment absent
        md.k0 = f64::NAN;
        md.k1 = f64::NAN;
        md.x0 = f64::NAN;
        apply_image_metadata(&mut pan, &md, 1000, 800);
        assert!(
            (pan.parms.k0 + 0.01).abs() < 1e-12,
            "profile should apply: k0={}",
            pan.parms.k0
        );
        // name is suggested regardless of the comment (C save dialog)
        assert_eq!(
            distortion_profile_name(&md).as_deref(),
            Some("Canon_EOS_5D_50.00_mm")
        );
        md.k0 = 0.0;
        assert_eq!(
            distortion_profile_name(&md).as_deref(),
            Some("Canon_EOS_5D_50.00_mm")
        );

        // 3. zero when neither
        md.k0 = f64::NAN;
        md.manufacturer = None;
        apply_image_metadata(&mut pan, &md, 1000, 800);
        assert_eq!(pan.parms.k0, 0.0);

        match old.0 {
            Some(h) => std::env::set_var("HOME", h),
            None => std::env::remove_var("HOME"),
        }
        match old.1 {
            Some(h) => std::env::set_var("XDG_DATA_HOME", h),
            None => std::env::remove_var("XDG_DATA_HOME"),
        }
        let _ = std::fs::remove_dir_all(&dir);
    }
}
