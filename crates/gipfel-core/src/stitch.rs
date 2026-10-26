//! Cylindrical panorama resampling — port of the C++ `Stitch.cxx` /
//! `ScanImage.cxx` stitch mode.
//!
//! The output is a cylindrical (equirectangular-in-azimuth, tan-in-elevation)
//! image spanning `view_start..view_end` degrees of azimuth. Each output
//! pixel is mapped back through every source image's fitted projection; the
//! first source that covers the direction contributes its (resampled) color.
//!
//! All sampling arithmetic reproduces the C implementation bit-for-bit:
//! colors are handled on the 0..=65025 scale (`value * 255`), nearest uses
//! `rint` (round-half-to-even), bicubic uses the same Keys cubic and only
//! rounds after the final vertical interpolation.

use crate::metadata::{ImageMetaData, apply_image_metadata};
use crate::panorama::Panorama;
use image::RgbImage;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum ScanMode {
    Nearest,
    Bicubic,
}

/// C MAX_VALUE: clamp bound on the 0..=65025 (value*255) scale.
pub const MAX_VALUE: i64 = 65025;

fn pixel(img: &RgbImage, x: i64, y: i64) -> Option<[i64; 3]> {
    if x < 0 || y < 0 || x >= img.width() as i64 || y >= img.height() as i64 {
        return None;
    }
    let p = img.get_pixel(x as u32, y as u32);
    Some([p[0] as i64 * 255, p[1] as i64 * 255, p[2] as i64 * 255])
}

/// C ScanImage::get_pixel_nearest: NaN-guard, then round to nearest pixel.
fn get_pixel_nearest(img: &RgbImage, x: f64, y: f64) -> Option<[i64; 3]> {
    if x.is_nan() || y.is_nan() {
        return None;
    }
    pixel(img, x.round_ties_even() as i64, y.round_ties_even() as i64)
}

/// C ScanImage interp_cubic (Keys cubic convolution kernel, evaluated in
/// Bernstein-like power basis).
#[inline]
fn interp_cubic(x: f64, x2: f64, x3: f64, v: &[f64; 4]) -> f64 {
    let a0 = v[3] - v[2] - v[0] + v[1];
    let a1 = v[0] - v[1] - a0;
    let a2 = v[2] - v[0];
    let a3 = v[1];
    a0 * x3 + a1 * x2 + a2 * x + a3
}

/// C ScanImage::get_pixel_bicubic. Any of the 16 taps outside the source
/// makes the pixel unset (C returns 1), matching the "leave black" behavior.
fn get_pixel_bicubic(img: &RgbImage, x: f64, y: f64) -> Option<[i64; 3]> {
    if x.is_nan() || y.is_nan() {
        return None;
    }
    let fl_x = x.floor();
    let fl_y = y.floor();
    let dx = x - fl_x;
    let dx2 = dx * dx;
    let dx3 = dx2 * dx;
    let dy = y - fl_y;
    let dy2 = dy * dy;
    let dy3 = dy2 * dy;

    let mut c1 = [[0f64; 4]; 3];
    for iy in 0..4usize {
        let mut c = [[0f64; 4]; 3];
        for ix in 0..4usize {
            let p = pixel(img, fl_x as i64 + ix as i64 - 1, fl_y as i64 + iy as i64 - 1)?;
            for l in 0..3 {
                c[l][ix] = p[l] as f64;
            }
        }
        for l in 0..3 {
            c1[l][iy] = interp_cubic(dx, dx2, dx3, &c[l]);
        }
    }
    Some([
        interp_cubic(dy, dy2, dy3, &c1[0]).round_ties_even() as i64,
        interp_cubic(dy, dy2, dy3, &c1[1]).round_ties_even() as i64,
        interp_cubic(dy, dy2, dy3, &c1[2]).round_ties_even() as i64,
    ])
}

/// C ScanImage::get_pixel(mode, x, y). Values on the 0..=65025 scale;
/// bicubic may overshoot (clamped by the caller, as in C).
pub fn get_pixel(img: &RgbImage, mode: ScanMode, x: f64, y: f64) -> Option<[i64; 3]> {
    match mode {
        ScanMode::Nearest => get_pixel_nearest(img, x, y),
        ScanMode::Bicubic => get_pixel_bicubic(img, x, y),
    }
}

/// One input photo: its fitted projection plus its decoded pixels.
pub struct StitchSource {
    pub pan: Panorama,
    pub img: RgbImage,
}

impl StitchSource {
    /// Load a JPEG and apply its gipfel metadata (comment or EXIF), like
    /// `GipfelWidget::load_image` does for the stitch mode.
    pub fn load(path: &str) -> Result<Self, String> {
        let img = image::open(path)
            .map(|i| i.to_rgb8())
            .map_err(|e| format!("{}: {e}", path))?;
        let mut md = ImageMetaData::new();
        md.load_image(path);
        let mut pan = Panorama::new();
        apply_image_metadata(&mut pan, &md, img.width(), img.height());
        Ok(Self { pan, img })
    }

    /// C GipfelWidget::get_pixel: forward-project (azimuth, elevation)
    /// through this image's projection and sample the source.
    pub fn get_pixel(&self, mode: ScanMode, a_alph: f64, a_elev: f64) -> Option<[i64; 3]> {
        let (px, py) = self.pan.project_point(a_alph, a_elev)?;
        get_pixel(
            &self.img,
            mode,
            px + self.img.width() as f64 / 2.0,
            py + self.img.height() as f64 / 2.0,
        )
    }
}

/// The resampled cylindrical panorama. Pixel values are RGB triplets on the
/// C 0..=65025 scale (clamped); `hit` marks pixels some source contributed
/// to (unset pixels are black in the C output).
pub struct StitchedImage {
    pub w: u32,
    pub h: u32,
    pub pixels: Vec<[u16; 3]>,
    pub hit: Vec<bool>,
}

/// Port of `Stitch::resample`: output column x maps linearly to azimuth,
/// output row y maps to elevation via `atan((y_off - y) / radius)`.
pub fn resample(
    sources: &[StitchSource],
    mode: ScanMode,
    w: u32,
    h: u32,
    view_start_deg: f64,
    view_end_deg: f64,
) -> StitchedImage {
    let view_start = view_start_deg.to_radians();
    let view_end = view_end_deg.to_radians();
    let mut pixels = vec![[0u16; 3]; (w as usize) * (h as usize)];
    let mut hit = vec![false; (w as usize) * (h as usize)];
    if !(view_end > view_start) || w == 0 || h == 0 {
        return StitchedImage {
            w,
            h,
            pixels,
            hit,
        };
    }
    let step_view = (view_end - view_start) / w as f64;
    let y_off = (h / 2) as f64;
    let radius = w as f64 / (view_end - view_start);

    for y in 0..h {
        let a_elev = ((y_off - y as f64) / radius).atan();
        for x in 0..w {
            let a_view = view_start + x as f64 * step_view;
            for src in sources {
                let Some(rgb) = src.get_pixel(mode, a_view, a_elev) else {
                    continue;
                };
                let clamped = [
                    rgb[0].clamp(0, MAX_VALUE) as u16,
                    rgb[1].clamp(0, MAX_VALUE) as u16,
                    rgb[2].clamp(0, MAX_VALUE) as u16,
                ];
                let i = (y as usize) * (w as usize) + (x as usize);
                pixels[i] = clamped;
                hit[i] = true;
                break; // C: first contributing image wins
            }
        }
    }

    StitchedImage {
        w,
        h,
        pixels,
        hit,
    }
}

/// Write an RGBA TIFF like the C `TIFFOutputImage`: 8 bit (value/255, alpha
/// 255) or 16 bit (raw 0..=65025, alpha 65025), deflate-compressed.
pub fn write_tiff(path: &str, img: &StitchedImage, bits16: bool) -> Result<(), String> {
    use tiff::encoder::colortype::{RGBA16, RGBA8};
    use tiff::encoder::{Compression, TiffEncoder};

    let n = (img.w as usize) * (img.h as usize);
    let file = std::fs::File::create(path).map_err(|e| format!("{}: {e}", path))?;
    let mut enc = TiffEncoder::new(file)
        .map_err(|e| format!("{}: {e}", path))?
        .with_compression(Compression::Deflate(Default::default()));
    if bits16 {
        let mut buf: Vec<u16> = Vec::with_capacity(n * 4);
        for p in &img.pixels {
            buf.extend_from_slice(&[p[0], p[1], p[2], MAX_VALUE as u16]);
        }
        enc.write_image::<RGBA16>(img.w, img.h, &buf)
            .map_err(|e| format!("{}: {e}", path))?;
    } else {
        let mut buf: Vec<u8> = Vec::with_capacity(n * 4);
        for p in &img.pixels {
            buf.extend_from_slice(&[(p[0] / 255) as u8, (p[1] / 255) as u8, (p[2] / 255) as u8, 255]);
        }
        enc.write_image::<RGBA8>(img.w, img.h, &buf)
            .map_err(|e| format!("{}: {e}", path))?;
    }
    Ok(())
}

/// Write a JPEG (quality 90, like the C `JPEGOutputImage`).
pub fn write_jpeg(path: &str, img: &StitchedImage, quality: u8) -> Result<(), String> {
    use image::codecs::jpeg::JpegEncoder;
    use image::ImageEncoder;

    let rgb: Vec<u8> = img
        .pixels
        .iter()
        .flat_map(|p| [(p[0] / 255) as u8, (p[1] / 255) as u8, (p[2] / 255) as u8])
        .collect();
    let enc = JpegEncoder::new_with_quality(
        std::io::BufWriter::new(std::fs::File::create(path).map_err(|e| format!("{}: {e}", path))?),
        quality,
    );
    enc.write_image(&rgb, img.w, img.h, image::ExtendedColorType::Rgb8)
        .map_err(|e| format!("{}: {e}", path))?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn solid(v: u8, w: u32, h: u32) -> RgbImage {
        RgbImage::from_pixel(w, h, image::Rgb([v, v, v]))
    }

    #[test]
    fn nearest_rounds_half_to_even() {
        let img = solid(100, 4, 4);
        // 1.5 -> 2 (ties to even), 2.5 -> 2
        assert_eq!(get_pixel_nearest(&img, 1.5, 0.0), Some([100 * 255; 3]));
        assert_eq!(get_pixel_nearest(&img, 2.5, 0.0), Some([100 * 255; 3]));
        assert_eq!(get_pixel_nearest(&img, -0.4, 0.0), Some([100 * 255; 3]));
        assert_eq!(get_pixel_nearest(&img, -0.6, 0.0), None);
        assert_eq!(get_pixel_nearest(&img, f64::NAN, 0.0), None);
    }

    #[test]
    fn bicubic_constant_and_bounds() {
        let img = solid(100, 4, 4);
        // constant image: cubic reproduces the constant exactly
        assert_eq!(get_pixel_bicubic(&img, 1.3, 1.7), Some([100 * 255; 3]));
        // any tap outside the 4x4 neighborhood -> unset
        assert_eq!(get_pixel_bicubic(&img, 0.0, 2.0), None); // fl_x-1 = -1
        assert_eq!(get_pixel_bicubic(&img, 2.0, 3.5), None); // fl_y+2 = 5
        assert_eq!(get_pixel_bicubic(&img, 1.0, 1.0), Some([100 * 255; 3]));
    }

    #[test]
    fn bicubic_matches_keys_cubic_on_ramp() {
        // Keys cubic (a=-1/2): on a linear ramp the kernel yields
        // a + b*(2 dx^3 - 3 dx^2 + 2 dx), not the linear value.
        let mut img = RgbImage::new(8, 8);
        for x in 0..8 {
            for y in 0..8 {
                img.put_pixel(x, y, image::Rgb([(x as u8) * 30, 0, 0]));
            }
        }
        let got = get_pixel_bicubic(&img, 3.25, 4.0).unwrap();
        let a = 3.0 * 30.0 * 255.0;
        let b = 30.0 * 255.0;
        let dx = 0.25f64;
        let want = (a + b * (2.0 * dx * dx * dx - 3.0 * dx * dx + 2.0 * dx)).round_ties_even();
        assert_eq!(got[0], want as i64);
    }

    #[test]
    fn bicubic_exact_at_integer_positions() {
        // at integer sample coordinates (dx = dy = 0) the cubic kernel
        // reproduces the center tap exactly -> a 1:1 resample is lossless
        let mut img = RgbImage::new(8, 8);
        for x in 0..8 {
            for y in 0..8 {
                img.put_pixel(x, y, image::Rgb([(x * 7 + y * 3) as u8, (y * 5) as u8, 9]));
            }
        }
        for y in 1..6 {
            for x in 1..6 {
                let p = img.get_pixel(x, y).0;
                assert_eq!(
                    get_pixel_bicubic(&img, x as f64, y as f64).unwrap(),
                    [p[0] as i64 * 255, p[1] as i64 * 255, p[2] as i64 * 255]
                );
            }
        }
    }

    #[test]
    fn resample_rejects_empty_range() {
        let img = StitchSource {
            pan: Panorama::new(),
            img: solid(50, 8, 8),
        };
        let out = resample(&[img], ScanMode::Nearest, 4, 4, 10.0, 10.0);
        assert_eq!(out.hit.len(), 16);
        assert!(out.hit.iter().all(|h| !h));
    }
}
