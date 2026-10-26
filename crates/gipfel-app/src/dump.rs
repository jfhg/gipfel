//! Screen dump: render markers/labels onto the photo and save as PNG/JPEG.
//! Replaces FLTK's window capture from the original "Screen Dump" menu item.

use image::RgbaImage;
use std::path::Path;

pub struct DumpMarker {
    pub x: f64,
    pub y: f64,
    pub name: String,
    pub known: bool,
    pub hidden: bool,
}

pub struct DumpTrackPt {
    pub x: f64,
    pub y: f64,
    pub hidden: bool,
    pub width: f64,
}

fn px(img: &mut RgbaImage, x: i64, y: i64, c: [u8; 4]) {
    if x >= 0 && y >= 0 && (x as u32) < img.width() && (y as u32) < img.height() {
        img.put_pixel(x as u32, y as u32, image::Rgba(c));
    }
}

fn line(img: &mut RgbaImage, x0: f64, y0: f64, x1: f64, y1: f64, c: [u8; 4], w: i64) {
    let dx = x1 - x0;
    let dy = y1 - y0;
    let n = (dx.hypot(dy) as i64).max(1);
    for i in 0..=n {
        let t = i as f64 / n as f64;
        let (x, y) = (x0 + dx * t, y0 + dy * t);
        for oy in -w..=w {
            for ox in -w..=w {
                px(img, x.round() as i64 + ox, y.round() as i64 + oy, c);
            }
        }
    }
}

fn circle(img: &mut RgbaImage, cx: f64, cy: f64, r: f64, c: [u8; 4]) {
    let n = (2.0 * std::f64::consts::PI * r.max(1.0)) as i64;
    for i in 0..=n {
        let a = i as f64 / n as f64 * 2.0 * std::f64::consts::PI;
        px(img, (cx + r * a.cos()).round() as i64, (cy + r * a.sin()).round() as i64, c);
        px(img, (cx + (r + 1.0) * a.cos()).round() as i64, (cy + (r + 1.0) * a.sin()).round() as i64, c);
    }
}

fn tri(img: &mut RgbaImage, p: [(f64, f64); 3], c: [u8; 4]) {
    let ymin = p.iter().map(|q| q.y() as i64).min().unwrap();
    let ymax = p.iter().map(|q| q.y() as i64).max().unwrap();
    for y in ymin..=ymax {
        let mut xmin = i64::MAX;
        let mut xmax = i64::MIN;
        for i in 0..3 {
            let a = p[i];
            let b = p[(i + 1) % 3];
            if (a.y() as i64) == (b.y() as i64) {
                continue;
            }
            if y >= (a.y() as i64).min(b.y() as i64) && y <= (a.y() as i64).max(b.y() as i64) {
                let t = (y as f64 - a.y()) / (b.y() - a.y());
                let x = a.x() + t * (b.x() - a.x());
                xmin = xmin.min(x.round() as i64);
                xmax = xmax.max(x.round() as i64);
            }
        }
        for x in xmin..=xmax {
            px(img, x, y, c);
        }
    }
}

trait XY {
    fn x(self) -> f64;
    fn y(self) -> f64;
}
impl XY for (f64, f64) {
    fn x(self) -> f64 {
        self.0
    }
    fn y(self) -> f64 {
        self.1
    }
}

fn load_font(family: &str) -> Option<fontdue::Font> {
    // Resolve the COSMIC interface font (or system sans-serif fallback) through
    // the same font discovery cosmic's text engine uses, then hand the bytes to
    // fontdue for the bitmap dump.
    static FS: std::sync::OnceLock<std::sync::Mutex<cosmic_text::FontSystem>> =
        std::sync::OnceLock::new();
    let mut fs = FS
        .get_or_init(|| std::sync::Mutex::new(cosmic_text::FontSystem::new()))
        .lock()
        .unwrap();
    let fam = [fontdb::Family::Name(family)];
    let id = fs
        .db()
        .query(&fontdb::Query { families: &fam, ..Default::default() })
        .or_else(|| fs.db().query(&fontdb::Query::default()))?;
    let font = fs.get_font(id, fontdb::Weight::NORMAL)?;
    fontdue::Font::from_bytes(
        font.data().to_vec(),
        fontdue::FontSettings { scale: 64.0, ..Default::default() },
    )
    .ok()
}

struct GlyphBlit {
    x: f32,
    y: f32,
    width: usize,
    #[allow(dead_code)]
    height: usize,
    pixels: Vec<u8>,
}

fn layout_text(font: &fontdue::Font, text: &str, size: f32) -> Vec<GlyphBlit> {
    use fontdue::layout::{CoordinateSystem, Layout as FontLayout, TextStyle};
    let mut l = FontLayout::new(CoordinateSystem::PositiveYDown);
    l.append(&[font], &TextStyle::new(text, size, 0));
    l.glyphs()
        .iter()
        .map(|g| {
            let (m, pixels) = font.rasterize_config(g.key);
            GlyphBlit { x: g.x, y: g.y, width: m.width, height: m.height, pixels }
        })
        .collect()
}

fn draw_text(img: &mut RgbaImage, font: &fontdue::Font, x: f64, y: f64, text: &str, c: [u8; 4], size: f32) {
    for glyph in layout_text(font, text, size) {
        let gx0 = (x + glyph.x as f64).round() as i64;
        let gy0 = (y + glyph.y as f64).round() as i64;
        for (i, &alpha) in glyph.pixels.iter().enumerate() {
            if alpha == 0 {
                continue;
            }
            let gx = gx0 + (i % glyph.width) as i64;
            let gy = gy0 + (i / glyph.width) as i64;
            if gx < 0 || gy < 0 || gx >= img.width() as i64 || gy >= img.height() as i64 {
                continue;
            }
            let a = alpha as u32;
            let old = img.get_pixel(gx as u32, gy as u32).0;
            let blend = |o: u8, n: u8| ((o as u32 * (255 - a) + n as u32 * a) / 255) as u8;
            px(img, gx, gy, [blend(old[0], c[0]), blend(old[1], c[1]), blend(old[2], c[2]), 255]);
        }
    }
}

fn text_width(font: &fontdue::Font, text: &str, size: f32) -> f64 {
    layout_text(font, text, size)
        .iter()
        .map(|g| (g.x + g.width as f32) as f64)
        .fold(0.0, f64::max)
}

/// Compose the dump: photo + crosses/leaders/labels + track.
///
/// `font_size` is the on-screen label font size in screen pixels, `scale`
/// converts screen pixels to image pixels (1 / zoom): the overlay geometry
/// is specified in screen pixels (as seen on the display) but the dump is
/// rendered at the image's native resolution, so every size must be scaled
/// accordingly. Label placement runs the exact same collision-avoiding
/// layout as the on-screen viewport (in screen space, then mapped back to
/// image space), so the dump shows the labels at the same positions.
pub fn render(
    img: &RgbaImage,
    markers: &[DumpMarker],
    track: &[DumpTrackPt],
    out: &Path,
    font_size: f32,
    scale: f64,
    font_family: &str,
) -> Result<(), String> {
    let mut canvas = img.clone();
    let (w, h) = (img.width() as f64, img.height() as f64);
    let (cx, cy) = (w / 2.0, h / 2.0);
    let s = scale.max(0.1);
    let font_img = f64::from(font_size) * s;
    let font = load_font(font_family);

    // track
    for win in track.windows(2) {
        let (a, b) = (&win[0], &win[1]);
        let c = if a.hidden { [51, 102, 255, 255] } else { [255, 51, 51, 255] };
        line(&mut canvas, cx + a.x, cy + a.y, cx + b.x, cy + b.y, c, (a.width as i64).max(1));
    }

    // label layout: the exact same collision-avoiding code as the on-screen
    // viewport (see viewport::layout_label_y). It runs in screen space
    // (marker positions / scale, screen font size); the resulting offsets
    // are mapped back to image space with s. Needed for the leader lines
    // and the label drawing below.
    let line_h = f64::from(font_size) * 1.3 + 2.0;
    let in_img: Vec<bool> = markers
        .iter()
        .map(|m| m.x.abs() * 2.0 <= w && m.y.abs() * 2.0 <= h)
        .collect();
    let layout: Option<Vec<(f32, f32)>> = font.as_ref().map(|font| {
        let sx: Vec<f32> = markers.iter().map(|m| (m.x / s) as f32).collect();
        let sy: Vec<f32> = markers.iter().map(|m| (m.y / s) as f32).collect();
        let widths: Vec<f32> = markers
            .iter()
            .map(|m| text_width(font, &m.name, font_size) as f32)
            .collect();
        crate::viewport::layout_label_y(&sx, &sy, &widths, &in_img, line_h as f32)
    });

    let yellow = [255, 230, 51, 255];
    // ~1 screen pixel stroke at image resolution
    let lw = ((s - 1.0) / 2.0).round().max(0.0) as i64;
    // like the screen: known-hill flags in red
    let flag_color = [255, 64, 64, 255];
    for (i, m) in markers.iter().enumerate() {
        let (x, y) = (cx + m.x, cy + m.y);
        if x < -10.0 || y < -10.0 || x > w + 10.0 || y > h + 10.0 || !in_img[i] {
            continue;
        }
        // yellow cross + leader line up to the label (screen section 3)
        let d = 4.0 * s;
        line(&mut canvas, x - d, y, x + d, y, yellow, lw);
        if let Some(lay) = &layout {
            let (lx, ly) = lay[i];
            let label_line = y + (f64::from(ly) - line_h) * s;
            line(&mut canvas, x, y, x, label_line, yellow, lw);
            line(&mut canvas, x, label_line, x + f64::from(lx) * s, label_line, yellow, lw);
        }
    }
    // known-hill flags (pole + pennant + circle), on top of all leaders
    for (i, m) in markers.iter().enumerate() {
        if !m.known || !in_img[i] {
            continue;
        }
        let (x, y) = (cx + m.x, cy + m.y);
        if x < -10.0 || y < -10.0 || x > w + 10.0 || y > h + 10.0 {
            continue;
        }
        let pole = 16.0 * s;
        line(&mut canvas, x, y, x, y - pole, flag_color, (s.round() as i64).max(1));
        tri(
            &mut canvas,
            [(x, y - pole), (x + 10.0 * s, y - pole * 0.8), (x, y - pole * 0.625)],
            flag_color,
        );
        circle(&mut canvas, x, y, 3.0 * s, flag_color);
    }

    if let (Some(font), Some(layout)) = (&font, &layout) {
        for (i, m) in markers.iter().enumerate() {
            if !in_img[i] {
                continue;
            }
            let (_, ly) = layout[i];
            let (x, y) = (cx + m.x + 2.0 * s, cy + m.y + (f64::from(ly) - line_h) * s);
            if x < 0.0 || y < 0.0 || x > w || y > h {
                continue;
            }
            let tw = text_width(font, &m.name, font_img as f32);
            for yy in (y - 2.0 * s) as i64..=(y + font_img + 2.0 * s) as i64 {
                for xx in (x - 1.0 * s) as i64..=(x + tw + 1.0 * s) as i64 {
                    px(&mut canvas, xx, yy, [0, 0, 0, 160]);
                }
            }
            let c = if m.hidden { [102, 153, 255, 255] } else { [255, 255, 255, 255] };
            draw_text(&mut canvas, font, x, y, &m.name, c, font_img as f32);
        }
    }

    if out
        .extension()
        .is_some_and(|e| e.eq_ignore_ascii_case("jpg") || e.eq_ignore_ascii_case("jpeg"))
    {
        let rgb = image::DynamicImage::ImageRgba8(canvas).to_rgb8();
        rgb.save(out.with_extension("jpg")).map_err(|e| e.to_string())?;
    } else {
        canvas.save(out.with_extension("png")).map_err(|e| e.to_string())?;
    }
    Ok(())
}
