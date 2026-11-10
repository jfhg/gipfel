// Dev helper: project hills for a given viewpoint and print image coords.
// usage: overlay <lat> <lon> <height> <az> <pitch> <roll> <focal35> <w> <h>
use gipfel_core::panorama::Panorama;

fn main() {
    let a: Vec<String> = std::env::args().collect();
    let n: Vec<f64> = a[1..].iter().map(|s| s.parse().unwrap()).collect();
    let (lat, lon, height, az, pitch, roll, focal, w, h) =
        (n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7], n[8]);
    let mut p = Panorama::new();
    p.load_data("gipfel.dat").unwrap();
    p.set_view_lat(lat);
    p.set_view_long(lon);
    p.set_view_height(height);
    p.set_scale(focal * w.max(h) / 36.0);
    p.set_center_angle(az);
    p.set_pitch_angle(pitch);
    p.set_roll_angle(roll);
    p.max_view_angle = 60_f64.to_radians();
    p.update_visible_mountains(None);
    println!("# az={az} pitch={pitch} scale={}", focal * w.max(h) / 36.0);
    for &i in &p.visible {
        let hill = &p.hills[i];
        let d = p.get_real_distance(i) / 1000.0;
        if hill.x.abs() < w / 2.0 && hill.y.abs() < h / 2.0 && hill.height > 1500.0 {
            // convert center-relative coords to full image coords
            println!("{:.0} {:.0} {} {:.0}m {:.1}km", hill.x + w / 2.0, hill.y + h / 2.0, hill.name, hill.height, d);
        }
    }
}
