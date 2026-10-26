//! Lens distortion profiles.
//!
//! Profiles live in the standard per-user data directory
//! (`$XDG_DATA_HOME/gipfel/distortion-profiles.prefs`, i.e.
//! `~/.local/share/gipfel/…`). The file format is the FLTK preferences
//! format used by the C version (`[./name]` groups, `key:value` entries,
//! `%g` values), so the legacy file
//! `~/.fltk/Johannes.HofmannATgmx.de/gipfel/DistortionProfiles.prefs`
//! imports automatically into the new location on first use.
//!
//! File layout (as written by FLTK):
//! ```text
//! ; FLTK preferences file format 1.0
//! ; vendor: Johannes.HofmannATgmx.de
//! ; application: gipfel/DistortionProfiles
//!
//! [.]
//!
//! [./Canon_EOS_5D_50.00_mm]
//!
//! k0:-0.00123
//! k1:2.5e-05
//! x0:0.5
//! ```

use std::path::{Path, PathBuf};

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Profile {
    pub k0: f64,
    pub k1: f64,
    pub x0: f64,
}

#[derive(Debug)]
pub enum Error {
    NotFound,
    Exists,
    BadName,
    Io(std::io::Error),
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::NotFound => write!(f, "profile not found"),
            Error::Exists => write!(f, "profile already exists"),
            Error::BadName => write!(f, "invalid profile name"),
            Error::Io(e) => write!(f, "{e}"),
        }
    }
}

/// The profile store in the standard data directory
/// (`$XDG_DATA_HOME/gipfel/distortion-profiles.prefs`).
pub fn prefs_path() -> PathBuf {
    dirs::data_dir()
        .unwrap_or_else(|| PathBuf::from(std::env::var("HOME").unwrap_or_default()))
        .join("gipfel")
        .join("distortion-profiles.prefs")
}

/// Legacy FLTK preferences file used by the C version.
fn fltk_prefs_path() -> PathBuf {
    let home = std::env::var("HOME").unwrap_or_default();
    Path::new(&home)
        .join(".fltk")
        .join("Johannes.HofmannATgmx.de")
        .join("gipfel")
        .join("DistortionProfiles.prefs")
}

/// Path to the profile store; imports legacy FLTK profiles on first use.
fn ensure_store() -> PathBuf {
    let path = prefs_path();
    if !path.exists() {
        let legacy = fltk_prefs_path();
        if legacy.exists() {
            if let Some(dir) = path.parent() {
                let _ = std::fs::create_dir_all(dir);
            }
            // same format, so a copy suffices
            let _ = std::fs::copy(&legacy, &path);
        }
    }
    path
}

/// C `get_distortion_profile_name`: `"%s_%s_%.2f_mm"`. `None` when the
/// camera is unknown or the focal length is unusable (C would build a name
/// containing "nan" that can never match a stored profile).
pub fn profile_name(manufacturer: &str, model: &str, focal_mm: f64) -> Option<String> {
    if manufacturer.is_empty() || model.is_empty() || !focal_mm.is_finite() || focal_mm <= 0.0 {
        return None;
    }
    let name = format!("{manufacturer}_{model}_{focal_mm:.2}_mm");
    sanitize_name(&name).ok().map(|_| name)
}

/// Reject names that would break the prefs file structure (FLTK group paths
/// use '/' as separator; control chars/newlines corrupt the file).
pub fn sanitize_name(name: &str) -> Result<(), Error> {
    if name.trim().is_empty() || name.len() > 255 {
        return Err(Error::BadName);
    }
    if name
        .chars()
        .any(|c| c == '/' || c == '\\' || c.is_control())
    {
        return Err(Error::BadName);
    }
    Ok(())
}

/// Parse a prefs file into ordered (name, profile) pairs. Tolerant:
/// comments (`;`), the root group `[.]`, blank lines and unknown keys are
/// ignored; values are parsed with Rust's `f64` (superset of C `%g`).
fn parse(text: &str) -> Vec<(String, Profile)> {
    let mut out: Vec<(String, Profile)> = Vec::new();
    let mut cur: Option<usize> = None;
    for line in text.lines() {
        let line = line.trim_end();
        if line.is_empty() || line.starts_with(';') {
            continue;
        }
        if let Some(group) = line.strip_prefix('[').and_then(|s| s.strip_suffix(']')) {
            cur = match group.strip_prefix("./") {
                Some(name) => {
                    out.push((
                        name.to_string(),
                        Profile {
                            k0: f64::NAN,
                            k1: f64::NAN,
                            x0: f64::NAN,
                        },
                    ));
                    Some(out.len() - 1)
                }
                None => None, // "[.]" root group
            };
            continue;
        }
        let Some(idx) = cur else { continue };
        if let Some((key, val)) = line.split_once(':') {
            let Ok(v) = val.trim().parse::<f64>() else {
                continue;
            };
            let p = &mut out[idx].1;
            match key.trim() {
                "k0" => p.k0 = v,
                "k1" => p.k1 = v,
                "x0" => p.x0 = v,
                _ => {}
            }
        }
    }
    out.retain(|(_, p)| p.k0.is_finite() && p.k1.is_finite() && p.x0.is_finite());
    out
}

/// Serialize in FLTK's layout so the C version can read our file.
fn serialize(profiles: &[(String, Profile)]) -> String {
    let mut s = String::from(
        "; FLTK preferences file format 1.0\n\
         ; vendor: Johannes.HofmannATgmx.de\n\
         ; application: gipfel/DistortionProfiles\n\
         \n\
         [.]\n\
         \n",
    );
    for (name, p) in profiles {
        s.push_str(&format!(
            "\n[./{name}]\n\nk0:{}\nk1:{}\nx0:{}\n",
            fmt_g(p.k0),
            fmt_g(p.k1),
            fmt_g(p.x0)
        ));
    }
    s
}

/// Closest Rust equivalent of C `%g`: scientific notation for magnitudes
/// outside [1e-4, 1e15), exponent zero-padded to two digits ("2.5e-05").
/// Unlike C we keep the shortest round-trip mantissa (more precise; the
/// FLTK reader uses strtod and reads either fine).
fn fmt_g(v: f64) -> String {
    if v != 0.0 && (v.abs() < 1e-4 || v.abs() >= 1e15) {
        if let Some((mant, exp)) = format!("{v:e}").split_once('e') {
            let neg = exp.starts_with('-');
            let digits = exp.trim_start_matches(['-', '+']);
            let digits = if digits.len() < 2 { format!("0{digits}") } else { digits.to_string() };
            let sign = if neg { '-' } else { '+' };
            return format!("{mant}e{sign}{digits}");
        }
    }
    if v == v.trunc() && v.abs() < 1e16 {
        format!("{}", v as i64)
    } else {
        format!("{v}")
    }
}

fn read_all(path: &Path) -> Result<Vec<(String, Profile)>, Error> {
    match std::fs::read_to_string(path) {
        Ok(text) => Ok(parse(&text)),
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(Vec::new()),
        Err(e) => Err(Error::Io(e)),
    }
}

fn write_all(path: &Path, profiles: &[(String, Profile)]) -> Result<(), Error> {
    if let Some(dir) = path.parent() {
        std::fs::create_dir_all(dir).map_err(Error::Io)?;
    }
    // atomic: temp file + rename
    let tmp = path.with_extension("prefs.tmp");
    std::fs::write(&tmp, serialize(profiles)).map_err(Error::Io)?;
    std::fs::rename(&tmp, path).map_err(Error::Io)?;
    Ok(())
}

/// Load a profile by exact name from the default store.
pub fn load(name: &str) -> Result<Profile, Error> {
    load_from(&ensure_store(), name)
}

pub fn load_from(path: &Path, name: &str) -> Result<Profile, Error> {
    read_all(path)?
        .into_iter()
        .find(|(n, _)| n == name)
        .map(|(_, p)| p)
        .ok_or(Error::NotFound)
}

/// Save a profile. With `force == false` an existing profile of that name is
/// not overwritten (C `save_distortion_params(name, force)`).
pub fn save(name: &str, profile: Profile, force: bool) -> Result<(), Error> {
    save_to(&ensure_store(), name, profile, force)
}

pub fn save_to(path: &Path, name: &str, profile: Profile, force: bool) -> Result<(), Error> {
    sanitize_name(name)?;
    let mut all = read_all(path)?;
    if let Some(pos) = all.iter().position(|(n, _)| n == name) {
        if !force {
            return Err(Error::Exists);
        }
        all[pos].1 = profile;
    } else {
        all.push((name.to_string(), profile));
    }
    write_all(path, &all)
}

/// All profiles in the default store, in file order.
pub fn list() -> Vec<(String, Profile)> {
    read_all(&ensure_store()).unwrap_or_default()
}

pub fn list_from(path: &Path) -> Vec<(String, Profile)> {
    read_all(path).unwrap_or_default()
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Exact bytes as written by the C version (verified against FLTK 1.4).
    const FLTK_FIXTURE: &str = "; FLTK preferences file format 1.0\n\
         ; vendor: Johannes.HofmannATgmx.de\n\
         ; application: gipfel/DistortionProfiles\n\
         \n\
         [.]\n\
         \n\
         \n\
         [./Canon_EOS_5D_50.00_mm]\n\
         \n\
         k0:-0.00123\n\
         k1:2.5e-05\n\
         x0:0.5\n\
         \n\
         [./Nikon D850_35.00_mm]\n\
         \n\
         k0:0.01\n\
         k1:0\n\
         x0:0\n";

    #[test]
    fn parses_fltk_layout() {
        let all = parse(FLTK_FIXTURE);
        assert_eq!(all.len(), 2);
        assert_eq!(all[0].0, "Canon_EOS_5D_50.00_mm");
        assert_eq!(all[0].1.k0, -0.00123);
        assert_eq!(all[0].1.k1, 2.5e-5);
        assert_eq!(all[0].1.x0, 0.5);
        assert_eq!(all[1].0, "Nikon D850_35.00_mm");
        assert_eq!(all[1].1.k0, 0.01);
        assert_eq!(all[1].1.k1, 0.0);
    }

    #[test]
    fn serialize_replicates_fltk_layout() {
        let all = parse(FLTK_FIXTURE);
        assert_eq!(serialize(&all), FLTK_FIXTURE);
    }

    #[test]
    fn fmt_g_matches_c_percent_g() {
        assert_eq!(fmt_g(2.5e-5), "2.5e-05");
        assert_eq!(fmt_g(-0.00123), "-0.00123");
        assert_eq!(fmt_g(0.0001), "0.0001");
        assert_eq!(fmt_g(1e15), "1e+15");
        assert_eq!(fmt_g(0.0), "0");
        assert_eq!(fmt_g(0.01), "0.01");
    }

    #[test]
    fn profile_name_matches_c() {
        assert_eq!(
            profile_name("Canon", "EOS_5D", 50.0).as_deref(),
            Some("Canon_EOS_5D_50.00_mm")
        );
        assert_eq!(
            profile_name("Nikon", "D850", 34.666).as_deref(),
            Some("Nikon_D850_34.67_mm")
        );
        assert!(profile_name("", "D850", 35.0).is_none());
        assert!(profile_name("Canon", "D850", f64::NAN).is_none());
        assert!(profile_name("Can/on", "D850", 35.0).is_none());
    }

    #[test]
    fn roundtrip_and_overwrite() {
        let dir = std::env::temp_dir().join(format!("gipfel-prof-{}", std::process::id()));
        let path = dir.join("DistortionProfiles.prefs");
        let _ = std::fs::remove_dir_all(&dir);

        let p = Profile { k0: -0.001, k1: 2e-5, x0: 0.0 };
        save_to(&path, "Cam_A_24.00_mm", p, false).unwrap();
        // exists -> error without force
        assert!(matches!(
            save_to(&path, "Cam_A_24.00_mm", p, false),
            Err(Error::Exists)
        ));
        save_to(&path, "Cam_B_50.00_mm", Profile { k0: 0.1, k1: 0.0, x0: 1.0 }, false).unwrap();
        // overwrite first, keep second
        let p2 = Profile { k0: -0.002, k1: 2e-5, x0: 0.0 };
        save_to(&path, "Cam_A_24.00_mm", p2, true).unwrap();

        let all = list_from(&path);
        assert_eq!(all.len(), 2);
        assert_eq!(all[0].1, p2);
        assert_eq!(load_from(&path, "Cam_B_50.00_mm").unwrap().k0, 0.1);
        assert!(matches!(load_from(&path, "nope"), Err(Error::NotFound)));

        // bad names rejected
        assert!(save_to(&path, "a/b", p, true).is_err());
        assert!(save_to(&path, "with\nnewline", p, true).is_err());

        std::fs::remove_dir_all(&dir).unwrap();
    }
}
