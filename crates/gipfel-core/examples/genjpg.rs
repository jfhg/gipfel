// creates a test JPEG with a gipfel comment: genjpg <out.jpg>
use gipfel_core::metadata::ImageMetaData;

fn main() {
    let out = std::env::args().nth(1).unwrap();
    let (w, h) = (1600u32, 900u32);
    let img = image::DynamicImage::new_rgb8(w, h).grayscale();
    img.save(&out).unwrap();

    let mut md = ImageMetaData::new();
    md.longitude = 11.15;
    md.latitude = 47.36;
    md.height = 2100.0;
    md.direction = 20.0;
    md.pitch = 0.0;
    md.roll = 1.0;
    md.focal_length_35mm = 35.0;
    md.projection_type = 0;
    md.k0 = 0.0;
    md.k1 = 0.0;
    md.x0 = 0.0;
    md.save_image(&out, &out).unwrap();
    println!("wrote {}", out);
}
